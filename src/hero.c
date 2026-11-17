
#include "client.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CARTE_FILE "carte.txt"
#define CARTE_PRECEDENTE "carte_precedente.txt"
#define JOURNAL "journal.txt"
#define DEBUT "debut"
#define MAX_MESSAGES 100000
#define MAX_MESSAGE 128
#define REPLY_SIZE (MAXREP + 1)

static const size_t CHEMIN_GAGNE[][2] = {
    {0, 2}, {1, 2}, {1, 2}, {0, 2}, {0, 2}, {1, 2}, {1, 2}, {0, 2}, {1, 2}, {0, 2},
    {0, 2}, {1, 2}, {0, 2}, {0, 1}, {0, 1}, {1, 2}, {1, 2}, {1, 2}, {0, 1}, {1, 2},
    {0, 1}, {0, 1}, {0, 2}, {2, 3}, {2, 3}, {0, 3}, {2, 3}, {2, 3}, {1, 3}, {2, 3},
    {0, 3}, {2, 3}, {0, 3}, {0, 2}, {3, 5}, {0, 1}, {1, 2}, {3, 4}, {4, 5}, {1, 5},
    {1, 2}, {0, 2}, {3, 5}, {0, 4}, {0, 18}, {0, 1}, {0, 21}, {0, 1}, {0, 22}, {0, 1},
    {0, 9}, {0, 1}, {0, 6}, {0, 1}, {0, 4}, {0, 1}, {0, 10}, {0, 1}, {0, 14}, {0, 1},
    {0, 4}, {0, 1}, {0, 4}, {0, 1}, {0, 6}, {0, 1}, {0, 5}, {0, 1}, {1, 24}, {1, 2},
    {0, 5}, {0, 1}, {0, 1}, {3, 5}, {2, 5}, {0, 1}, {2, 5}, {4, 5}, {1, 2}, {0, 1},
    {79, 144}, {109, 145}};
#define CHEMIN_GAGNE_LEN (sizeof CHEMIN_GAGNE / sizeof CHEMIN_GAGNE[0])

static char reponse[REPLY_SIZE];

static void die(const char *message) {
    fprintf(stderr, "hero: %s\n", message);
    exit(1);
}

static void *xmalloc(size_t size) {
    void *memory = malloc(size);
    if (memory == NULL) die("out of memory");
    return memory;
}

static char *xstrndup(const char *text, size_t length) {
    char *copy = xmalloc(length + 1);
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static char *xstrdup(const char *text) {
    return xstrndup(text, strlen(text));
}

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} Path;

static void path_push(Path *path, const char *id) {
    if (path->len == path->cap) {
        size_t cap = path->cap ? 2 * path->cap : 8;
        char **items = realloc(path->items, cap * sizeof *items);
        if (items == NULL) die("out of memory");
        path->items = items;
        path->cap = cap;
    }
    path->items[path->len++] = xstrdup(id);
}

static Path path_copy(const Path *path) {
    Path copy = {NULL, 0, 0};
    for (size_t i = 0; i < path->len; i++) path_push(&copy, path->items[i]);
    return copy;
}

static void path_free(Path *path) {
    for (size_t i = 0; i < path->len; i++) free(path->items[i]);
    free(path->items);
    path->items = NULL;
    path->len = 0;
    path->cap = 0;
}

static int path_starts_with(const Path *path, const Path *prefix) {
    if (prefix->len > path->len) return 0;
    for (size_t i = 0; i < prefix->len; i++) {
        if (strcmp(path->items[i], prefix->items[i]) != 0) return 0;
    }
    return 1;
}

typedef struct {
    char *id;
    char **options;
    size_t nopt;
    char *text;
} Page;

typedef struct {
    Page *pages;
    size_t n;
    size_t cap;
    size_t *slots;
    size_t nslots;
} Map;

static size_t hash_id(const char *id) {
    size_t hash = 1469598103934665603ULL;
    for (; *id != '\0'; id++) {
        hash ^= (unsigned char)*id;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static long map_find(const Map *map, const char *id) {
    if (map->nslots == 0) return -1;
    size_t mask = map->nslots - 1;
    size_t slot = hash_id(id) & mask;
    while (map->slots[slot] != 0) {
        size_t index = map->slots[slot] - 1;
        if (strcmp(map->pages[index].id, id) == 0) return (long)index;
        slot = (slot + 1) & mask;
    }
    return -1;
}

static void map_index(Map *map, size_t index) {
    size_t mask = map->nslots - 1;
    size_t slot = hash_id(map->pages[index].id) & mask;
    while (map->slots[slot] != 0) slot = (slot + 1) & mask;
    map->slots[slot] = index + 1;
}

static void map_grow(Map *map) {
    size_t nslots = map->nslots ? 2 * map->nslots : 128;
    while (map->n * 2 > nslots) nslots *= 2;
    free(map->slots);
    map->slots = calloc(nslots, sizeof *map->slots);
    if (map->slots == NULL) die("out of memory");
    map->nslots = nslots;
    for (size_t index = 0; index < map->n; index++) map_index(map, index);
}

static void page_free_contents(Page *page) {
    for (size_t i = 0; i < page->nopt; i++) free(page->options[i]);
    free(page->options);
    free(page->text);
    page->options = NULL;
    page->nopt = 0;
    page->text = NULL;
}

static void map_put(Map *map, const char *id, char **options, size_t nopt, char *text) {
    long index = map_find(map, id);
    if (index >= 0) {
        Page *page = &map->pages[index];
        page_free_contents(page);
        page->options = options;
        page->nopt = nopt;
        page->text = text;
        return;
    }
    if (map->n == map->cap) {
        size_t cap = map->cap ? 2 * map->cap : 64;
        Page *pages = realloc(map->pages, cap * sizeof *pages);
        if (pages == NULL) die("out of memory");
        map->pages = pages;
        map->cap = cap;
    }
    Page *page = &map->pages[map->n];
    page->id = xstrdup(id);
    page->options = options;
    page->nopt = nopt;
    page->text = text;
    map->n++;
    if (map->n * 2 > map->nslots) {
        map_grow(map);
    } else {
        map_index(map, map->n - 1);
    }
}

static void map_clear(Map *map) {
    for (size_t index = 0; index < map->n; index++) {
        free(map->pages[index].id);
        page_free_contents(&map->pages[index]);
    }
    free(map->pages);
    free(map->slots);
    memset(map, 0, sizeof *map);
}

static size_t split_options(const char *list, char ***out) {
    size_t count = 0, cap = 0;
    char **options = NULL;
    const char *start = list;
    for (;;) {
        const char *end = strchr(start, ',');
        size_t length = end ? (size_t)(end - start) : strlen(start);
        if (length > 0) {
            if (count == cap) {
                cap = cap ? 2 * cap : 4;
                options = realloc(options, cap * sizeof *options);
                if (options == NULL) die("out of memory");
            }
            options[count++] = xstrndup(start, length);
        }
        if (end == NULL) break;
        start = end + 1;
    }
    *out = options;
    return count;
}

static void unescape_newlines(char *text) {
    char *read = text, *write = text;
    while (*read != '\0') {
        if (read[0] == '\\' && read[1] == 'n') {
            *write++ = '\n';
            read += 2;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

static void load_map(Map *map, const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        if (errno == ENOENT) return;
        perror(path);
        exit(1);
    }
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    while ((length = getline(&line, &capacity, file)) != -1) {
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        char *tab1 = strchr(line, '\t');
        if (tab1 == NULL) continue;
        *tab1 = '\0';
        char *tab2 = strchr(tab1 + 1, '\t');
        if (tab2 == NULL) continue;
        *tab2 = '\0';
        char **options;
        size_t nopt = split_options(tab1 + 1, &options);
        char *text = xstrdup(tab2 + 1);
        unescape_newlines(text);
        map_put(map, line, options, nopt, text);
    }
    free(line);
    fclose(file);
}

static int compare_page_ids(const void *a, const void *b) {
    const Page *const *pa = a;
    const Page *const *pb = b;
    return strcmp((*pa)->id, (*pb)->id);
}

static void save_map(const Map *map, const char *path) {
    Page **sorted = xmalloc((map->n ? map->n : 1) * sizeof *sorted);
    for (size_t index = 0; index < map->n; index++) sorted[index] = &map->pages[index];
    qsort(sorted, map->n, sizeof *sorted, compare_page_ids);

    char temporary[512];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    FILE *file = fopen(temporary, "w");
    if (file == NULL) {
        perror(temporary);
        exit(1);
    }
    for (size_t index = 0; index < map->n; index++) {
        const Page *page = sorted[index];
        fprintf(file, "%s\t", page->id);
        for (size_t i = 0; i < page->nopt; i++) {
            fprintf(file, "%s%s", i ? "," : "", page->options[i]);
        }
        fputc('\t', file);
        for (const char *c = page->text; *c != '\0'; c++) {
            if (*c == '\n') {
                fputs("\\n", file);
            } else {
                fputc(*c, file);
            }
        }
        fputc('\n', file);
    }
    fclose(file);
    if (rename(temporary, path) != 0) {
        perror(path);
        exit(1);
    }
    free(sorted);
}

static char *leading_id(const char *text) {
    size_t length = 0;
    while (isalnum((unsigned char)text[length])) length++;
    return xstrndup(text, length);
}

static size_t parse_options(const char *text, char ***out) {
    static const char CHOICE[] = "entre la page ";
    static const char NEXT[] = "la page ";
    static const char MARKERS[][40] = {"continue en page ", "continue en allant à la page "};

    char **ids = NULL;
    size_t count = 0, cap = 0;
    const char *choice = strstr(text, CHOICE);
    if (choice != NULL) {
        choice += strlen(CHOICE);
        const char *dot = strchr(choice, '.');
        size_t length = dot ? (size_t)(dot - choice) : strlen(choice);
        char *list = xstrndup(choice, length);
        char *cursor = list;
        for (;;) {
            char *id = leading_id(cursor);
            if (id[0] != '\0') {
                if (count == cap) {
                    cap = cap ? 2 * cap : 4;
                    ids = realloc(ids, cap * sizeof *ids);
                    if (ids == NULL) die("out of memory");
                }
                ids[count++] = id;
            } else {
                free(id);
            }
            char *next = strstr(cursor, NEXT);
            if (next == NULL) break;
            cursor = next + strlen(NEXT);
        }
        free(list);
        if (count > 0) {
            *out = ids;
            return count;
        }
    }
    for (size_t m = 0; m < sizeof MARKERS / sizeof MARKERS[0]; m++) {
        const char *found = strstr(text, MARKERS[m]);
        if (found == NULL) continue;
        char *id = leading_id(found + strlen(MARKERS[m]));
        if (id[0] != '\0') {
            ids = xmalloc(sizeof *ids);
            ids[0] = id;
            *out = ids;
            return 1;
        }
        free(id);
    }
    *out = NULL;
    return 0;
}

static size_t count_options(const char *text) {
    char **options;
    size_t count = parse_options(text, &options);
    for (size_t i = 0; i < count; i++) free(options[i]);
    free(options);
    return count;
}

static void say(const char *message) {
    if (strlen(message) + 4 > MAX_MESSAGE) die("message too long");
    char buffer[MAX_MESSAGE];
    strcpy(buffer, message);
    if (envoyer_recevoir(buffer, reponse) < 0) die("connection lost");
}

static char *play_path(const Path *path) {
    say("start");
    if (strstr(reponse, "choisissez un nom") != NULL) say("spectre");
    for (size_t i = 0; i < path->len; i++) say(path->items[i]);
    return xstrdup(reponse);
}

static void login(const char *credentials) {
    connexion((char *)getenv("APPOLAB_HOST"));
    const char *line = credentials;
    while (*line != '\0') {
        const char *newline = strchr(line, '\n');
        size_t length = newline ? (size_t)(newline - line) + 1 : strlen(line);
        char *text = xstrndup(line, length);
        say(text);
        free(text);
        line += length;
    }
    say("load hero");
}

static char *advance(Path *game, int *game_active, const Path *target, size_t *sent) {
    char *text;
    if (*game_active && path_starts_with(target, game) && target->len > game->len) {
        for (size_t i = game->len; i < target->len; i++) say(target->items[i]);
        *sent = target->len - game->len;
        text = xstrdup(reponse);
    } else {
        text = play_path(target);
        *sent = target->len + 2;
    }
    path_free(game);
    *game = path_copy(target);
    *game_active = count_options(text) > 0;
    return text;
}

typedef struct {
    char *id;
    Path path;
} Target;

static void path_to_node(const Map *map, const long *parent, long node, Path *out) {
    size_t depth = 0;
    for (long c = node; parent[c] != -1; c = parent[c]) depth++;
    const char **ids = xmalloc((depth + 1) * sizeof *ids);
    size_t i = 0;
    for (long c = node; parent[c] != -1; c = parent[c]) ids[i++] = map->pages[c].id;
    for (size_t j = depth; j > 0; j--) path_push(out, ids[j - 1]);
    free(ids);
}

static int next_target_bfs(const Map *map, Target *out) {
    long root = map_find(map, DEBUT);
    if (root < 0) {
        out->id = xstrdup(DEBUT);
        out->path = (Path){NULL, 0, 0};
        return 1;
    }
    size_t n = map->n;
    long *parent = xmalloc(n * sizeof *parent);
    size_t *queue = xmalloc(n * sizeof *queue);
    char *seen = calloc(n, 1);
    if (seen == NULL) die("out of memory");
    for (size_t k = 0; k < n; k++) parent[k] = -2;
    size_t head = 0, tail = 0;
    int found = 0;
    parent[root] = -1;
    seen[root] = 1;
    queue[tail++] = (size_t)root;
    while (head < tail && !found) {
        long current = (long)queue[head++];
        const Page *page = &map->pages[current];
        for (size_t i = 0; i < page->nopt && !found; i++) {
            const char *option = page->options[i];
            long index = map_find(map, option);
            if (index < 0) {
                out->id = xstrdup(option);
                out->path = (Path){NULL, 0, 0};
                path_to_node(map, parent, current, &out->path);
                path_push(&out->path, option);
                found = 1;
            } else if (!seen[index]) {
                seen[index] = 1;
                parent[index] = current;
                queue[tail++] = (size_t)index;
            }
        }
    }
    free(parent);
    free(queue);
    free(seen);
    return found;
}

static int known_path(const Map *map, const char *target, Path *out) {
    long root = map_find(map, DEBUT);
    if (root < 0) return 0;
    size_t n = map->n;
    long *parent = xmalloc(n * sizeof *parent);
    size_t *queue = xmalloc(n * sizeof *queue);
    char *seen = calloc(n, 1);
    if (seen == NULL) die("out of memory");
    size_t head = 0, tail = 0;
    int found = 0;
    parent[root] = -1;
    seen[root] = 1;
    queue[tail++] = (size_t)root;
    while (head < tail && !found) {
        long current = (long)queue[head++];
        const Page *page = &map->pages[current];
        if (strcmp(page->id, target) == 0) {
            path_to_node(map, parent, current, out);
            found = 1;
            break;
        }
        for (size_t i = 0; i < page->nopt; i++) {
            long index = map_find(map, page->options[i]);
            if (index >= 0 && !seen[index]) {
                seen[index] = 1;
                parent[index] = current;
                queue[tail++] = (size_t)index;
            }
        }
    }
    free(parent);
    free(queue);
    free(seen);
    return found;
}

typedef struct {
    long index;
    char *id;
    Path path;
} DfsItem;

typedef struct {
    DfsItem *items;
    size_t len;
    size_t cap;
} DfsStack;

static void dfs_push(DfsStack *stack, long index, const char *id, Path path) {
    if (stack->len == stack->cap) {
        size_t cap = stack->cap ? 2 * stack->cap : 64;
        DfsItem *items = realloc(stack->items, cap * sizeof *items);
        if (items == NULL) die("out of memory");
        stack->items = items;
        stack->cap = cap;
    }
    stack->items[stack->len++] = (DfsItem){index, xstrdup(id), path};
}

static void dfs_free_all(DfsStack *stack) {
    for (size_t i = 0; i < stack->len; i++) {
        free(stack->items[i].id);
        path_free(&stack->items[i].path);
    }
    free(stack->items);
    stack->items = NULL;
    stack->len = 0;
    stack->cap = 0;
}

static int next_target_dfs(const Map *map, int right_first, const char *zone, Target *out) {
    if (map_find(map, DEBUT) < 0) {
        out->id = xstrdup(DEBUT);
        out->path = (Path){NULL, 0, 0};
        return 1;
    }
    DfsStack stack = {NULL, 0, 0};
    char *expanded = calloc(map->n ? map->n : 1, 1);
    if (expanded == NULL) die("out of memory");

    if (zone != NULL) {
        Path start = {NULL, 0, 0};
        if (!known_path(map, zone, &start)) {
            free(expanded);
            return 0;
        }
        dfs_push(&stack, map_find(map, zone), zone, start);
    } else {
        dfs_push(&stack, map_find(map, DEBUT), DEBUT, (Path){NULL, 0, 0});
    }

    while (stack.len > 0) {
        DfsItem item = stack.items[--stack.len];
        if (item.index < 0) {
            out->id = item.id;
            out->path = item.path;
            dfs_free_all(&stack);
            free(expanded);
            return 1;
        }
        if (expanded[item.index]) {
            free(item.id);
            path_free(&item.path);
            continue;
        }
        expanded[item.index] = 1;
        const Page *page = &map->pages[item.index];
        for (size_t j = 0; j < page->nopt; j++) {
            size_t i = right_first ? j : page->nopt - 1 - j;
            Path child = path_copy(&item.path);
            path_push(&child, page->options[i]);
            dfs_push(&stack, map_find(map, page->options[i]), page->options[i], child);
        }
        free(item.id);
        path_free(&item.path);
    }
    free(expanded);
    return 0;
}

static int path_to_win(const Map *map, Path *out) {
    long root = map_find(map, DEBUT);
    if (root < 0) return 0;
    size_t n = map->n;
    long *parent = xmalloc(n * sizeof *parent);
    size_t *queue = xmalloc(n * sizeof *queue);
    char *seen = calloc(n, 1);
    if (seen == NULL) die("out of memory");
    size_t head = 0, tail = 0;
    int found = 0;
    parent[root] = -1;
    seen[root] = 1;
    queue[tail++] = (size_t)root;
    while (head < tail && !found) {
        long current = (long)queue[head++];
        const Page *page = &map->pages[current];
        if (page->nopt == 0 && strstr(page->text, "gagné") != NULL) {
            path_to_node(map, parent, current, out);
            found = 1;
            break;
        }
        for (size_t i = 0; i < page->nopt; i++) {
            long index = map_find(map, page->options[i]);
            if (index >= 0 && !seen[index]) {
                seen[index] = 1;
                parent[index] = current;
                queue[tail++] = (size_t)index;
            }
        }
    }
    free(parent);
    free(queue);
    free(seen);
    return found;
}

static size_t pending_options(const Map *map) {
    size_t count = 0;
    for (size_t index = 0; index < map->n; index++) {
        for (size_t i = 0; i < map->pages[index].nopt; i++) {
            if (map_find(map, map->pages[index].options[i]) < 0) count++;
        }
    }
    return count;
}

static void print_oneline(FILE *out, const char *text) {
    for (; *text != '\0'; text++) fputc(*text == '\n' ? ' ' : *text, out);
}

static void free_strings(char **items, size_t count) {
    for (size_t i = 0; i < count; i++) free(items[i]);
    free(items);
}

static int compare_strings(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int parse_number(const char *text, unsigned long long *value) {
    if (*text == '\0') return 0;
    for (const char *c = text; *c != '\0'; c++) {
        if (!isdigit((unsigned char)*c)) return 0;
    }
    errno = 0;
    *value = strtoull(text, NULL, 10);
    return errno == 0;
}

static void mode_help(const char *credentials) {
    login(credentials);
    say("aide");
    printf("%s\n", reponse);
}

static void mode_replay(const char *credentials, const char *choices) {
    Map map = {NULL, 0, 0, NULL, 0};
    load_map(&map, CARTE_FILE);
    const char *current = DEBUT;
    Path path = {NULL, 0, 0};
    for (const char *letter = choices ? choices : ""; *letter != '\0'; letter++) {
        long index = map_find(&map, current);
        if (index < 0) die("page not in the map");
        size_t choice = 0;
        if (*letter == 'L') {
            choice = 0;
        } else if (*letter == 'R') {
            choice = 1;
        } else {
            die("the path may only contain L and R");
        }
        Page *page = &map.pages[index];
        if (choice >= page->nopt) die("missing option");
        path_push(&path, page->options[choice]);
        current = page->options[choice];
    }
    login(credentials);
    char *text = play_path(&path);
    printf("%s\n", text);
    free(text);
    path_free(&path);
    map_clear(&map);
}

static void mode_win(const char *credentials) {
    login(credentials);
    Path empty = {NULL, 0, 0};
    char *text = play_path(&empty);
    for (size_t step = 0; step < CHEMIN_GAGNE_LEN; step++) {
        char **options;
        size_t count = parse_options(text, &options);
        size_t position = CHEMIN_GAGNE[step][0];
        size_t expected = CHEMIN_GAGNE[step][1];
        if (count != expected || position >= count) {
            printf("route differs at step %zu: %zu options instead of %zu: ", step + 1, count, expected);
            print_oneline(stdout, text);
            printf("\n");
            free(text);
            free_strings(options, count);
            return;
        }
        say(options[position]);
        free(text);
        free_strings(options, count);
        text = xstrdup(reponse);
    }
    printf("final page: ");
    print_oneline(stdout, text);
    printf("\n");
    free(text);
    say("perlimpimpin");
    printf("reply to perlimpimpin: ");
    print_oneline(stdout, reponse);
    printf("\n");
}

typedef struct {
    char *id;
    Path path;
} Probe;

static void mode_probe(const char *credentials) {
    const size_t budget_limit = 5000;
    login(credentials);
    Path empty = {NULL, 0, 0};
    char *root = play_path(&empty);
    size_t budget = 2;
    printf("root: ");
    print_oneline(stdout, root);
    printf("\n");

    char **root_options;
    size_t root_count = parse_options(root, &root_options);
    Probe *stack = NULL;
    size_t len = 0, cap = 0;

    for (size_t i = root_count; i > 0; i--) {
        unsigned long long value;
        if (!parse_number(root_options[i - 1], &value)) continue;
        if (len == cap) {
            cap = cap ? 2 * cap : 16;
            stack = realloc(stack, cap * sizeof *stack);
            if (stack == NULL) die("out of memory");
        }
        stack[len].id = xstrdup(root_options[i - 1]);
        stack[len].path = (Path){NULL, 0, 0};
        path_push(&stack[len].path, root_options[i - 1]);
        len++;
    }
    free_strings(root_options, root_count);
    free(root);

    size_t endings = 0;
    while (len > 0) {
        if (budget > budget_limit) {
            printf("budget reached (%zu messages)\n", budget_limit);
            break;
        }
        Probe probe = stack[--len];
        char *text = play_path(&probe.path);
        budget += probe.path.len + 2;
        char **options;
        size_t count = parse_options(text, &options);
        unsigned long long current = 0;
        parse_number(probe.id, &current);
        printf("[%3zu] %s | options:", probe.path.len, probe.id);
        for (size_t i = 0; i < count; i++) printf(" %s", options[i]);
        printf(" | ");
        char summary[111];
        strncpy(summary, text, 110);
        summary[110] = '\0';
        print_oneline(stdout, summary);
        printf("\n");

        if (count == 0) {
            endings++;
            if (strstr(text, "gagné") != NULL && strstr(text, "Ha bah non") == NULL) {
                printf("   !!! unexpected win text: ");
                print_oneline(stdout, text);
                printf("\n");
            }
            if (strstr(text, "terminé") != NULL || strstr(text, "relancer") != NULL) {
                printf("   !!! end of session: ");
                print_oneline(stdout, text);
                printf("\n");
                free(text);
                free(probe.id);
                path_free(&probe.path);
                break;
            }
        }
        for (size_t i = count; i > 0; i--) {
            unsigned long long value;
            if (!parse_number(options[i - 1], &value) || value <= current) continue;
            if (len == cap) {
                cap = cap ? 2 * cap : 16;
                stack = realloc(stack, cap * sizeof *stack);
                if (stack == NULL) die("out of memory");
            }
            stack[len].id = xstrdup(options[i - 1]);
            stack[len].path = path_copy(&probe.path);
            path_push(&stack[len].path, options[i - 1]);
            len++;
        }
        free(text);
        free_strings(options, count);
        free(probe.id);
        path_free(&probe.path);
    }
    for (size_t i = 0; i < len; i++) {
        free(stack[i].id);
        path_free(&stack[i].path);
    }
    free(stack);
    printf("endings reached: %zu, messages: %zu\n", endings, budget);
}

static void mode_explore(const char *credentials, const char *mode, const char *zone) {
    int play_win = strcmp(mode, "complet") == 0 || strcmp(mode, "right") == 0;
    int right_first = play_win;
    int depth_first = strcmp(mode, "dfs") == 0 || play_win || zone != NULL;

    if (zone != NULL) {
        Map check = {NULL, 0, 0, NULL, 0};
        load_map(&check, CARTE_FILE);
        if (map_find(&check, zone) < 0) {
            fprintf(stderr, "hero: page %s is not in the map\n", zone);
            exit(1);
        }
        map_clear(&check);
    }

    login(credentials);
    Map map = {NULL, 0, 0, NULL, 0};
    load_map(&map, CARTE_FILE);

    size_t messages = 0, new_pages = 0;
    const char *reason = "exploration complete";
    int root_changed = 0;
    Path empty = {NULL, 0, 0};

    char *root = play_path(&empty);
    messages += 2;
    struct timespec started;
    clock_gettime(CLOCK_MONOTONIC, &started);
    Path game = {NULL, 0, 0};
    int game_active = count_options(root) > 0;

    char **root_options;
    size_t root_count = parse_options(root, &root_options);
    qsort(root_options, root_count, sizeof *root_options, compare_strings);
    long root_index = map_find(&map, DEBUT);
    int same_root = 1;
    if (root_index >= 0) {
        const Page *saved = &map.pages[root_index];
        char **saved_options = xmalloc((saved->nopt ? saved->nopt : 1) * sizeof *saved_options);
        memcpy(saved_options, saved->options, saved->nopt * sizeof *saved_options);
        qsort(saved_options, saved->nopt, sizeof *saved_options, compare_strings);
        same_root = saved->nopt == root_count;
        for (size_t i = 0; same_root && i < root_count; i++) {
            if (strcmp(saved_options[i], root_options[i]) != 0) same_root = 0;
        }
        free(saved_options);
    }
    if (map.n > 0 && !same_root) {
        if (zone != NULL) die("the root has changed: the chosen page is not in this map");
        root_changed = 1;
        if (rename(CARTE_FILE, CARTE_PRECEDENTE) != 0) {
            perror(CARTE_FILE);
            exit(1);
        }
        map_clear(&map);
    }
    free_strings(root_options, root_count);
    if (map_find(&map, DEBUT) < 0) {
        char **options;
        size_t count = parse_options(root, &options);
        map_put(&map, DEBUT, options, count, xstrdup(root));
        save_map(&map, CARTE_FILE);
    }

    for (;;) {
        Target target = {NULL, {NULL, 0, 0}};
        int found = depth_first ? next_target_dfs(&map, right_first, zone, &target)
                                : next_target_bfs(&map, &target);
        if (!found) break;
        if (messages > MAX_MESSAGES) {
            reason = "message limit reached";
            free(target.id);
            path_free(&target.path);
            break;
        }
        size_t sent = 0;
        char *text = advance(&game, &game_active, &target.path, &sent);
        messages += sent;
        if (strstr(text, "L'exercice est terminé") != NULL) {
            reason = "session ended by the server";
            free(text);
            free(target.id);
            path_free(&target.path);
            break;
        }
        if (strstr(text, "réponds") != NULL) {
            printf("WARNING: question asked on %s: nothing is sent, the text is kept in the map\n", target.id);
        }
        int relaunch = strstr(text, "relancer l'exercice") != NULL;
        char **options;
        size_t count = parse_options(text, &options);
        map_put(&map, target.id, options, count, text);
        new_pages++;
        save_map(&map, CARTE_FILE);
        if (new_pages % 25 == 0) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double minutes = ((double)(now.tv_sec - started.tv_sec) + (double)(now.tv_nsec - started.tv_nsec) / 1e9) / 60.0;
            if (minutes < 1e-9) minutes = 1e-9;
            printf("[%6.1f min] %zu pages known, %zu messages, %.1f pages/min, %zu options waiting (minimum left)\n",
                   minutes, map.n, messages, (double)new_pages / minutes, pending_options(&map));
            fflush(stdout);
        }
        free(target.id);
        path_free(&target.path);
        if (relaunch) {
            reason = "dissolved (session over)";
            break;
        }
    }

    if (play_win) {
        Path win = {NULL, 0, 0};
        if (path_to_win(&map, &win)) {
            char *text = play_path(&win);
            printf("--- nearest win (%zu pages) ---\n%s\n", win.len, text);
            free(text);
        } else {
            printf("no win page in the map\n");
        }
        path_free(&win);
    }

    const char *root_note = root_changed ? "root changed: previous map saved in carte_precedente.txt"
                                         : "same root as the saved map";
    char summary[1024];
    snprintf(summary, sizeof summary, "%zu messages, %zu new pages, %zu pages known, stopped: %s (%s)",
             messages, new_pages, map.n, reason, root_note);
    printf("%s\n", summary);
    FILE *journal = fopen(JOURNAL, "a");
    if (journal != NULL) {
        fprintf(journal, "%s\n", summary);
        fclose(journal);
    }
    free(root);
    path_free(&game);
    map_clear(&map);
}

static char *read_credentials(void) {
    FILE *file = fopen(".env", "r");
    if (file == NULL) die("cannot read .env");
    size_t length = 0, cap = 256;
    char *contents = xmalloc(cap);
    int c;
    while ((c = fgetc(file)) != EOF) {
        if (length + 1 == cap) {
            cap *= 2;
            contents = realloc(contents, cap);
            if (contents == NULL) die("out of memory");
        }
        contents[length++] = (char)c;
    }
    fclose(file);
    contents[length] = '\0';
    int blank = 1;
    for (size_t i = 0; i < length; i++) {
        if (!isspace((unsigned char)contents[i])) blank = 0;
    }
    if (blank) die(".env is empty");
    return contents;
}

int main(int argc, char **argv) {
    char *credentials = read_credentials();
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "aide") == 0) {
        mode_help(credentials);
    } else if (strcmp(mode, "rejouer") == 0) {
        mode_replay(credentials, argc > 2 ? argv[2] : NULL);
    } else if (strcmp(mode, "gagner") == 0) {
        mode_win(credentials);
    } else if (strcmp(mode, "sonder") == 0) {
        mode_probe(credentials);
    } else {
        mode_explore(credentials, mode, argc > 2 ? argv[2] : NULL);
    }
    free(credentials);
    deconnexion();
    return 0;
}
