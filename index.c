#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// ─── Load index ───────────────────────────────────────

int index_load(Index *index)
{
    index->count = 0;

    FILE *fp = fopen(".pes/index", "r");
    if (!fp) return 0;

    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];

        char hash_hex[HASH_HEX_SIZE + 1];

        if (fscanf(fp, "%o %64s %u %s\n",
                   &e->mode,
                   hash_hex,
                   &e->size,
                   e->path) != 4)
            break;

        hex_to_hash(hash_hex, &e->hash);
        index->count++;
    }

    fclose(fp);
    return 0;
}

// ─── Save index ───────────────────────────────────────

int index_save(const Index *index)
{
    FILE *fp = fopen(".pes/index.tmp", "w");
    if (!fp) return -1;

    for (int i = 0; i < index->count; i++) {
        const IndexEntry *e = &index->entries[i];

        char hash_hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&e->hash, hash_hex);

        fprintf(fp, "%06o %s %u %s\n",
                e->mode,
                hash_hex,
                e->size,
                e->path);
    }

    fclose(fp);
    rename(".pes/index.tmp", ".pes/index");
    return 0;
}

// ─── Add file ─────────────────────────────────────────

int index_add(Index *index, const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    void *buffer = malloc(size);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    ObjectID hash;
    object_write(OBJ_BLOB, buffer, size, &hash);

    free(buffer);

    IndexEntry *e = &index->entries[index->count];

    e->mode = 0100644;
    e->size = (uint32_t)size;
    strncpy(e->path, path, sizeof(e->path));
    e->hash = hash;

    index->count++;

    // ✅ CRITICAL FIX: save index immediately
    index_save(index);

    return 0;
}

// ─── Status ───────────────────────────────────────────

int index_status(const Index *index)
{
    printf("Staged changes:\n");

    if (index->count == 0) {
        printf("  (nothing to show)\n");
        return 0;
    }

    for (int i = 0; i < index->count; i++) {
        printf("  staged: %s\n", index->entries[i].path);
    }

    return 0;
}
