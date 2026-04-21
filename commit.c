#include "commit.h"
#include "pes.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ─── HEAD helpers ─────────────────────────────────────

int head_read(ObjectID *id_out)
{
    FILE *f = fopen(".pes/refs/heads/main", "r");
    if (!f) return -1;

    char hex[HASH_HEX_SIZE + 1];
    if (!fgets(hex, sizeof(hex), f)) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return hex_to_hash(hex, id_out);
}

int head_update(const ObjectID *id)
{
    FILE *f = fopen(".pes/refs/heads/main", "w");
    if (!f) return -1;

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);

    fprintf(f, "%s\n", hex);
    fclose(f);
    return 0;
}

// ─── Commit creation ─────────────────────────────────

int commit_create(const char *message, ObjectID *commit_id_out)
{
    ObjectID tree_id;
    if (tree_from_index(&tree_id) != 0) return -1;

    ObjectID parent_id;
    int has_parent = (head_read(&parent_id) == 0);

    const char *author = pes_author();
    unsigned long long timestamp = (unsigned long long)time(NULL);

    char tree_hex[HASH_HEX_SIZE + 1];
    hash_to_hex(&tree_id, tree_hex);

    char parent_hex[HASH_HEX_SIZE + 1];
    if (has_parent)
        hash_to_hex(&parent_id, parent_hex);

    char buffer[2048];

    if (has_parent) {
        snprintf(buffer, sizeof(buffer),
                 "tree %s\nparent %s\nauthor %s\ntimestamp %llu\n\n%s\n",
                 tree_hex, parent_hex, author, timestamp, message);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "tree %s\nauthor %s\ntimestamp %llu\n\n%s\n",
                 tree_hex, author, timestamp, message);
    }

    if (object_write(OBJ_COMMIT, buffer, strlen(buffer), commit_id_out) != 0)
        return -1;

    if (head_update(commit_id_out) != 0)
        return -1;

    return 0;
}

// ─── Commit parsing ──────────────────────────────────

int commit_parse(const void *data, size_t len, Commit *commit)
{
    (void)len;

    memset(commit, 0, sizeof(*commit));

    const char *text = (const char *)data;

    char tree_hex[HASH_HEX_SIZE + 1] = {0};
    char parent_hex[HASH_HEX_SIZE + 1] = {0};

    sscanf(text, "tree %64s", tree_hex);
    hex_to_hash(tree_hex, &commit->tree);

    char *parent = strstr(text, "parent ");
    if (parent) {
        sscanf(parent, "parent %64s", parent_hex);
        hex_to_hash(parent_hex, &commit->parent);
    }

    char *author = strstr(text, "author ");
    if (author)
        sscanf(author, "author %[^\n]", commit->author);

    char *ts = strstr(text, "timestamp ");
    if (ts)
        sscanf(ts, "timestamp %lu", &commit->timestamp);

    char *msg = strstr(text, "\n\n");
    if (msg)
        strncpy(commit->message, msg + 2, sizeof(commit->message));

    return 0;
}

// ─── Walk commits ────────────────────────────────────

int commit_walk(void (*cb)(const ObjectID *, const Commit *, void *), void *ctx)
{
    ObjectID current;

    if (head_read(&current) != 0)
        return -1;

    while (1) {
        void *data;
        size_t len;
        ObjectType type;

        if (object_read(&current, &type, &data, &len) != 0)
            return -1;

        Commit commit;
        commit_parse(data, len, &commit);

        cb(&current, &commit, ctx);

        free(data);

        // Stop if no parent (all zero hash)
        int is_zero = 1;
        for (int i = 0; i < HASH_SIZE; i++) {
            if (commit.parent.hash[i] != 0) {
                is_zero = 0;
                break;
            }
        }

        if (is_zero)
            break;

        current = commit.parent;
    }

    return 0;
}
