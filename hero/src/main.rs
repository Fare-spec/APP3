use std::collections::{BTreeMap, HashMap, HashSet, VecDeque};
use std::env;
use std::fs;
use std::io::{self, Error, ErrorKind, Read, Write};
use std::net::{TcpStream, ToSocketAddrs};
use std::os::fd::AsRawFd;
use std::time::{Duration, Instant};

const HOST: &str = "im2ag-appolab.u-ga.fr";
const PORT: u16 = 443;
const MAX_RESPONSE: usize = 100_000;
const MAX_MESSAGE: usize = 128;
const MAX_MESSAGES: usize = 100_000;
const CARTE_FILE: &str = "carte.txt";
const DEBUT: &str = "debut";

const CHEMIN_GAGNE: [(usize, usize); 82] = [(0, 2), (1, 2), (1, 2), (0, 2), (0, 2), (1, 2), (1, 2), (0, 2), (1, 2), (0, 2), (0, 2), (1, 2), (0, 2), (0, 1), (0, 1), (1, 2), (1, 2), (1, 2), (0, 1), (1, 2), (0, 1), (0, 1), (0, 2), (2, 3), (2, 3), (0, 3), (2, 3), (2, 3), (1, 3), (2, 3), (0, 3), (2, 3), (0, 3), (0, 2), (3, 5), (0, 1), (1, 2), (3, 4), (4, 5), (1, 5), (1, 2), (0, 2), (3, 5), (0, 4), (0, 18), (0, 1), (0, 21), (0, 1), (0, 22), (0, 1), (0, 9), (0, 1), (0, 6), (0, 1), (0, 4), (0, 1), (0, 10), (0, 1), (0, 14), (0, 1), (0, 4), (0, 1), (0, 4), (0, 1), (0, 6), (0, 1), (0, 5), (0, 1), (1, 24), (1, 2), (0, 5), (0, 1), (0, 1), (3, 5), (2, 5), (0, 1), (2, 5), (4, 5), (1, 2), (0, 1), (79, 144), (109, 145)];

#[cfg(target_os = "linux")]
const TCP_QUICKACK: i32 = 12;

#[cfg(target_os = "linux")]
const IPPROTO_TCP: i32 = 6;

#[cfg(target_os = "linux")]
unsafe extern "C" {
    fn setsockopt(
        socket: std::os::fd::RawFd,
        level: i32,
        option_name: i32,
        option_value: *const core::ffi::c_void,
        option_len: u32,
    ) -> i32;
}

struct Appolab {
    stream: TcpStream,
}

impl Appolab {
    fn connect(address: &str) -> io::Result<Self> {
        let stream = TcpStream::connect(address)?;
        stream.set_nodelay(true)?;
        stream.set_read_timeout(Some(Duration::from_secs(60)))?;
        stream.set_write_timeout(Some(Duration::from_secs(60)))?;
        let mut client = Self { stream };
        client.read_welcome()?;
        client.send_packet_mode()?;
        Ok(client)
    }

    fn set_quick_ack(&self) -> io::Result<()> {
        #[cfg(target_os = "linux")]
        {
            let enabled: i32 = 1;
            let result = unsafe {
                setsockopt(
                    self.stream.as_raw_fd(),
                    IPPROTO_TCP,
                    TCP_QUICKACK,
                    &enabled as *const _ as *const core::ffi::c_void,
                    size_of::<i32>() as u32,
                )
            };
            if result != 0 {
                return Err(Error::last_os_error());
            }
        }
        #[cfg(not(target_os = "linux"))]
        let _ = self;
        Ok(())
    }

    fn read_quick(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
        let count = self.stream.read(buffer)?;
        if count > 0 {
            self.set_quick_ack()?;
        }
        Ok(count)
    }

    fn read_exact_quick(&mut self, buffer: &mut [u8]) -> io::Result<()> {
        let mut filled = 0;
        while filled < buffer.len() {
            let count = self.read_quick(&mut buffer[filled..])?;
            if count == 0 {
                return Err(Error::new(ErrorKind::UnexpectedEof, "connection closed"));
            }
            filled += count;
        }
        Ok(())
    }

    fn read_u32(&mut self) -> io::Result<u32> {
        let mut header = [0; 4];
        self.read_exact_quick(&mut header)?;
        Ok(u32::from_be_bytes(header))
    }

    fn read_welcome(&mut self) -> io::Result<()> {
        let mut buffer = [0; 32_000];
        let mut welcome = Vec::new();
        loop {
            let count = self.read_quick(&mut buffer)?;
            if count == 0 {
                return Err(Error::new(ErrorKind::UnexpectedEof, "welcome ended early"));
            }
            welcome.extend_from_slice(&buffer[..count]);
            if welcome.ends_with(b"\n\n") {
                return Ok(());
            }
            if welcome.len() > MAX_RESPONSE {
                return Err(Error::new(ErrorKind::InvalidData, "welcome too long"));
            }
        }
    }

    fn send_packet_mode(&mut self) -> io::Result<()> {
        self.stream.write_all(&0xFFFF01CC_u32.to_be_bytes())?;
        self.read_u32()?;
        Ok(())
    }

    fn send_receive_into(&mut self, message: &[u8], response: &mut [u8]) -> io::Result<usize> {
        if message.len() + 4 > MAX_MESSAGE {
            return Err(Error::new(
                ErrorKind::InvalidInput,
                format!("message too long: {}", message.len()),
            ));
        }
        let mut packet = [0u8; MAX_MESSAGE];
        packet[..4].copy_from_slice(&(message.len() as u32).to_be_bytes());
        packet[4..4 + message.len()].copy_from_slice(message);
        self.stream.write_all(&packet[..4 + message.len()])?;

        let size = usize::try_from(self.read_u32()?)
            .map_err(|_| Error::new(ErrorKind::InvalidData, "invalid response size"))?;
        if size > MAX_RESPONSE {
            return Err(Error::new(
                ErrorKind::InvalidData,
                format!("server response too long: {size}"),
            ));
        }
        self.read_exact_quick(&mut response[..size])?;
        Ok(size)
    }
}

fn first_address() -> io::Result<String> {
    let host = env::var("APPOLAB_HOST").unwrap_or_else(|_| HOST.to_string());
    let port = env::var("APPOLAB_PORT")
        .ok()
        .and_then(|port| port.parse::<u16>().ok())
        .unwrap_or(PORT);
    format!("{host}:{port}")
        .to_socket_addrs()?
        .next()
        .map(|address| address.to_string())
        .ok_or_else(|| Error::new(ErrorKind::NotFound, "server address not found"))
}

fn login(address: &str, credentials: &str, response: &mut [u8]) -> io::Result<Appolab> {
    let mut client = Appolab::connect(address)?;
    for line in credentials.split_inclusive('\n') {
        client.send_receive_into(line.as_bytes(), response)?;
    }
    send(&mut client, response, b"load hero")?;
    Ok(client)
}

fn send(client: &mut Appolab, response: &mut [u8], message: &[u8]) -> io::Result<usize> {
    client.send_receive_into(message, response)
}

fn reply_text(response: &[u8], size: usize) -> String {
    String::from_utf8_lossy(&response[..size]).into_owned()
}

fn play_path(client: &mut Appolab, response: &mut [u8], path: &[String]) -> io::Result<String> {
    let mut size = send(client, response, b"start")?;
    if reply_text(response, size).contains("choisissez un nom") {
        size = send(client, response, b"spectre")?;
    }
    for page in path {
        size = send(client, response, page.as_bytes())?;
    }
    Ok(reply_text(response, size))
}

fn advance(
    client: &mut Appolab,
    response: &mut [u8],
    game: &mut Vec<String>,
    game_active: &mut bool,
    path: &[String],
) -> io::Result<(String, usize)> {
    let continues = path.starts_with(game) && path.len() > game.len();
    let (text, sent) = if *game_active && continues {
        let mut size = 0;
        for page in &path[game.len()..] {
            size = send(client, response, page.as_bytes())?;
        }
        (reply_text(response, size), path.len() - game.len())
    } else {
        (play_path(client, response, path)?, path.len() + 2)
    };
    *game = path.to_vec();
    *game_active = !parse_options(&text).is_empty();
    Ok((text, sent))
}

fn parse_options(text: &str) -> Vec<String> {
    if let Some((_, rest)) = text.split_once("entre la page ") {
        let choices = rest.split('.').next().unwrap_or("");
        let ids: Vec<String> = choices
            .split("la page ")
            .map(|piece| {
                piece
                    .chars()
                    .take_while(|c| c.is_ascii_alphanumeric())
                    .collect::<String>()
            })
            .filter(|page| !page.is_empty())
            .collect();
        if !ids.is_empty() {
            return ids;
        }
    }
    for marker in ["continue en page ", "continue en allant à la page "] {
        if let Some((_, rest)) = text.split_once(marker) {
            let page: String = rest
                .chars()
                .take_while(|c| c.is_ascii_alphanumeric())
                .collect();
            if !page.is_empty() {
                return vec![page];
            }
        }
    }
    Vec::new()
}

struct Page {
    options: Vec<String>,
    text: String,
}

type Map = BTreeMap<String, Page>;

fn load_map(path: &str) -> io::Result<Map> {
    let contents = match fs::read_to_string(path) {
        Ok(contents) => contents,
        Err(error) if error.kind() == ErrorKind::NotFound => return Ok(Map::new()),
        Err(error) => return Err(error),
    };
    let mut map = Map::new();
    for line in contents.lines() {
        let mut fields = line.splitn(3, '\t');
        let (Some(id), Some(options), Some(text)) = (fields.next(), fields.next(), fields.next())
        else {
            continue;
        };
        let options = options
            .split(',')
            .filter(|option| !option.is_empty())
            .map(str::to_string)
            .collect();
        map.insert(
            id.to_string(),
            Page {
                options,
                text: text.replace("\\n", "\n"),
            },
        );
    }
    Ok(map)
}

fn save_map(path: &str, map: &Map) -> io::Result<()> {
    let mut contents = String::new();
    for (id, page) in map {
        contents.push_str(&format!(
            "{id}\t{}\t{}\n",
            page.options.join(","),
            page.text.replace('\n', "\\n")
        ));
    }
    fs::write(path, contents)
}

fn path_to(parents: &HashMap<String, String>, target: &str) -> Vec<String> {
    let mut path = Vec::new();
    let mut current = target.to_string();
    while current != DEBUT {
        path.push(current.clone());
        match parents.get(&current) {
            Some(parent) => current = parent.clone(),
            None => break,
        }
    }
    path.reverse();
    path
}

fn next_target_bfs(map: &Map) -> Option<(String, Vec<String>)> {
    if !map.contains_key(DEBUT) {
        return Some((DEBUT.to_string(), Vec::new()));
    }
    let mut parents: HashMap<String, String> = HashMap::new();
    let mut seen: HashSet<String> = HashSet::from([DEBUT.to_string()]);
    let mut queue: VecDeque<String> = VecDeque::from([DEBUT.to_string()]);
    while let Some(current) = queue.pop_front() {
        let Some(page) = map.get(&current) else {
            continue;
        };
        for option in &page.options {
            if !map.contains_key(option) {
                let mut path = path_to(&parents, &current);
                path.push(option.clone());
                return Some((option.clone(), path));
            }
            if seen.insert(option.clone()) {
                parents.insert(option.clone(), current.clone());
                queue.push_back(option.clone());
            }
        }
    }
    None
}

fn known_path(map: &Map, target: &str) -> Option<Vec<String>> {
    let mut parents: HashMap<String, String> = HashMap::new();
    let mut seen: HashSet<String> = HashSet::from([DEBUT.to_string()]);
    let mut queue: VecDeque<String> = VecDeque::from([DEBUT.to_string()]);
    while let Some(current) = queue.pop_front() {
        if current == target {
            return Some(path_to(&parents, &current));
        }
        let Some(page) = map.get(&current) else {
            continue;
        };
        for option in &page.options {
            if seen.insert(option.clone()) {
                parents.insert(option.clone(), current.clone());
                queue.push_back(option.clone());
            }
        }
    }
    None
}

fn next_target_dfs(
    map: &Map,
    right_first: bool,
    zone: Option<&str>,
) -> Option<(String, Vec<String>)> {
    if !map.contains_key(DEBUT) {
        return Some((DEBUT.to_string(), Vec::new()));
    }
    let start = match zone {
        None => (DEBUT.to_string(), Vec::new()),
        Some(page) => (page.to_string(), known_path(map, page)?),
    };
    let mut stack: Vec<(String, Vec<String>)> = vec![start];
    let mut expanded: HashSet<String> = HashSet::new();
    while let Some((current, path)) = stack.pop() {
        if !map.contains_key(&current) {
            return Some((current, path));
        }
        if !expanded.insert(current.clone()) {
            continue;
        }
        let Some(page) = map.get(&current) else {
            continue;
        };
        let options: Box<dyn Iterator<Item = &String>> = if right_first {
            Box::new(page.options.iter())
        } else {
            Box::new(page.options.iter().rev())
        };
        for option in options {
            let mut child_path = path.clone();
            child_path.push(option.clone());
            stack.push((option.clone(), child_path));
        }
    }
    None
}

fn pending_options(map: &Map) -> usize {
    map.values()
        .flat_map(|page| page.options.iter())
        .filter(|option| !map.contains_key(*option))
        .count()
}

fn path_to_win(map: &Map) -> Option<Vec<String>> {
    let mut parents: HashMap<String, String> = HashMap::new();
    let mut seen: HashSet<String> = HashSet::from([DEBUT.to_string()]);
    let mut queue: VecDeque<String> = VecDeque::from([DEBUT.to_string()]);
    while let Some(current) = queue.pop_front() {
        let Some(page) = map.get(&current) else {
            continue;
        };
        if page.options.is_empty() && page.text.contains("gagné") {
            return Some(path_to(&parents, &current));
        }
        for option in &page.options {
            if seen.insert(option.clone()) {
                parents.insert(option.clone(), current.clone());
                queue.push_back(option.clone());
            }
        }
    }
    None
}

fn main() {
    let credentials = match fs::read_to_string(".env") {
        Ok(contents) if !contents.trim().is_empty() => contents,
        Ok(_) => {
            eprintln!("hero setup: .env is empty");
            std::process::exit(1);
        }
        Err(error) => {
            eprintln!("hero setup: cannot read .env: {error}");
            std::process::exit(1);
        }
    };
    let address = match first_address() {
        Ok(address) => address,
        Err(error) => {
            eprintln!("cannot resolve the server: {error}");
            std::process::exit(1);
        }
    };

    let arguments: Vec<String> = env::args().collect();
    let mode = arguments.get(1).map(String::as_str).unwrap_or("");
    let result = match mode {
        "aide" => ask_for_help(&address, &credentials),
        "rejouer" => replay(&address, &credentials, arguments.get(2).map(String::as_str)),
        "gagner" => win_route(&address, &credentials),
        "sonder" => probe(&address, &credentials),
        _ => explore(&address, &credentials, mode, arguments.get(2).cloned()),
    };
    if let Err(error) = result {
        eprintln!("hero: {error}");
        std::process::exit(1);
    }
}

fn ask_for_help(address: &str, credentials: &str) -> io::Result<()> {
    let mut response = vec![0u8; MAX_RESPONSE];
    let mut client = login(address, credentials, &mut response)?;
    let size = send(&mut client, &mut response, b"aide")?;
    println!("{}", reply_text(&response, size));
    Ok(())
}

fn replay(address: &str, credentials: &str, choices: Option<&str>) -> io::Result<()> {
    let map = load_map(CARTE_FILE)?;
    let mut current = DEBUT.to_string();
    let mut path = Vec::new();
    for letter in choices.unwrap_or("").chars() {
        let page = map
            .get(&current)
            .ok_or_else(|| Error::other("page not in the map"))?;
        let index = match letter {
            'L' => 0,
            'R' => 1,
            _ => return Err(Error::other("path must only contain L and R")),
        };
        let next = page
            .options
            .get(index)
            .ok_or_else(|| Error::other("missing option"))?
            .clone();
        path.push(next.clone());
        current = next;
    }

    let mut response = vec![0u8; MAX_RESPONSE];
    let mut client = login(address, credentials, &mut response)?;
    let text = play_path(&mut client, &mut response, &path)?;
    println!("{text}");
    Ok(())
}

fn win_route(address: &str, credentials: &str) -> io::Result<()> {
    let mut response = vec![0u8; MAX_RESPONSE];
    let mut client = login(address, credentials, &mut response)?;

    let mut text = play_path(&mut client, &mut response, &[])?;
    for (step, &(position, expected)) in CHEMIN_GAGNE.iter().enumerate() {
        let options = parse_options(&text);
        if options.len() != expected || position >= options.len() {
            println!(
                "route differs at step {} : {} options instead of {expected} : {}",
                step + 1,
                options.len(),
                text.replace('\n', " ")
            );
            return Ok(());
        }
        let size = send(&mut client, &mut response, options[position].as_bytes())?;
        text = reply_text(&response, size);
    }
    println!("final page : {}", text.replace('\n', " "));
    let size = send(&mut client, &mut response, b"perlimpimpin")?;
    println!(
        "reply to perlimpimpin : {}",
        reply_text(&response, size).replace('\n', " ")
    );
    Ok(())
}

fn probe(address: &str, credentials: &str) -> io::Result<()> {
    const BUDGET: usize = 5000;
    let mut response = vec![0u8; MAX_RESPONSE];
    let mut client = login(address, credentials, &mut response)?;

    let root = play_path(&mut client, &mut response, &[])?;
    let mut budget = 2usize;
    println!("root : {}", root.replace('\n', " "));

    let increasing = |options: &[String], current: u64| -> Vec<String> {
        options
            .iter()
            .filter(|option| option.parse::<u64>().is_ok_and(|value| value > current))
            .cloned()
            .collect()
    };
    let mut stack: Vec<(String, Vec<String>)> = Vec::new();
    for option in increasing(&parse_options(&root), 0).into_iter().rev() {
        stack.push((option.clone(), vec![option]));
    }

    let mut endings = 0usize;
    while let Some((page, path)) = stack.pop() {
        if budget > BUDGET {
            println!("budget reached ({BUDGET} messages)");
            break;
        }
        let text = play_path(&mut client, &mut response, &path)?;
        budget += path.len() + 2;
        let options = parse_options(&text);
        let current: u64 = page.parse().unwrap_or(0);
        let summary: String = text.replace('\n', " ").chars().take(110).collect();
        println!("[{:>3}] {page} | options {:?} | {summary}", path.len(), options);
        if options.is_empty() {
            endings += 1;
            if text.contains("gagné") && !text.contains("Ha bah non") {
                println!("   !!! unexpected win text : {}", text.replace('\n', " "));
            }
            if text.contains("terminé") || text.contains("relancer") {
                println!("   !!! end of session : {}", text.replace('\n', " "));
                break;
            }
        }
        for option in increasing(&options, current).into_iter().rev() {
            let mut child_path = path.clone();
            child_path.push(option.clone());
            stack.push((option, child_path));
        }
    }
    println!("endings reached : {endings}, messages : {budget}");
    Ok(())
}

fn explore(
    address: &str,
    credentials: &str,
    mode: &str,
    zone: Option<String>,
) -> io::Result<()> {
    let play_win = mode == "complet" || mode == "right";
    let right_first = play_win;
    let depth_first = mode == "dfs" || play_win || zone.is_some();

    if let Some(page) = &zone {
        let map = load_map(CARTE_FILE)?;
        if !map.contains_key(page) {
            return Err(Error::other(format!("page {page} is not in the map")));
        }
    }

    let mut response = vec![0u8; MAX_RESPONSE];
    let mut client = login(address, credentials, &mut response)?;

    let mut map = load_map(CARTE_FILE)?;
    let mut messages = 0usize;
    let mut new_pages = 0usize;
    let mut reason = String::from("exploration complete");
    let mut root_changed = false;

    let root = play_path(&mut client, &mut response, &[])?;
    messages += 2;
    let run_start = Instant::now();
    let mut game: Vec<String> = Vec::new();
    let mut game_active = !parse_options(&root).is_empty();
    let mut root_options = parse_options(&root);
    root_options.sort();
    let mut known_root_options = map
        .get(DEBUT)
        .map(|page| page.options.clone())
        .unwrap_or_default();
    known_root_options.sort();
    if !map.is_empty() && known_root_options != root_options {
        if zone.is_some() {
            return Err(Error::other(
                "the root has changed: the chosen page is not in this map",
            ));
        }
        root_changed = true;
        fs::rename(CARTE_FILE, "carte_precedente.txt")?;
        map = Map::new();
    }
    if !map.contains_key(DEBUT) {
        map.insert(
            DEBUT.to_string(),
            Page {
                options: parse_options(&root),
                text: root,
            },
        );
        save_map(CARTE_FILE, &map)?;
    }

    loop {
        let next = if depth_first {
            next_target_dfs(&map, right_first, zone.as_deref())
        } else {
            next_target_bfs(&map)
        };
        let Some((target, path)) = next else {
            break;
        };
        if messages > MAX_MESSAGES {
            reason = String::from("message limit reached");
            break;
        }
        let (text, sent) = advance(&mut client, &mut response, &mut game, &mut game_active, &path)?;
        messages += sent;
        if text.contains("L'exercice est terminé") {
            reason = String::from("session ended by the server");
            break;
        }
        let options = parse_options(&text);
        if text.contains("réponds") {
            println!("WARNING: question asked on {target}: nothing is sent, the text is kept in the map");
        }
        map.insert(target.clone(), Page { options, text: text.clone() });
        new_pages += 1;
        save_map(CARTE_FILE, &map)?;
        if new_pages % 25 == 0 {
            let minutes = run_start.elapsed().as_secs_f64() / 60.0;
            println!(
                "[{:>6.1} min] {} pages known, {} messages, {:.1} pages/min, {} options waiting (minimum left)",
                minutes,
                map.len(),
                messages,
                new_pages as f64 / minutes.max(1e-9),
                pending_options(&map)
            );
        }
        if text.contains("relancer l'exercice") {
            reason = String::from("dissolved (session over)");
            break;
        }
    }

    if play_win {
        match path_to_win(&map) {
            Some(path) => {
                let text = play_path(&mut client, &mut response, &path)?;
                println!("--- nearest win ({} pages) ---\n{text}", path.len());
            }
            None => println!("no win page in the map"),
        }
    }

    let root_note = if root_changed {
        "root changed: previous map saved in carte_precedente.txt"
    } else {
        "same root as the saved map"
    };
    let summary = format!(
        "{messages} messages, {new_pages} new pages, {} pages known, stopped: {reason} ({root_note})",
        map.len()
    );
    println!("{summary}");
    let mut journal = fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open("journal.txt")?;
    writeln!(journal, "{summary}")?;
    Ok(())
}
