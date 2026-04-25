#include "uidl_store.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void uidl_store_extract_ids(const char *chunk, size_t chunk_length, FILE *out) {
    if (chunk == NULL || out == NULL) {
        return;
    }

    bool past_first_space = false;
    for (size_t index = 0; index < chunk_length; ++index) {
        const char current = chunk[index];

        if (current == '\n') {
            fputc('\n', out);
            past_first_space = false;
            continue;
        }

        if (current == '\r') {
            continue;
        }

        if (!past_first_space) {
            if (current == ' ') {
                past_first_space = true;
            }
            continue;
        }

        if (current != ' ') {
            fputc(current, out);
        }
    }
}

int uidl_store_count_new(FILE *known, FILE *current) {
    if (current == NULL) {
        return 0;
    }

    char current_id[256];
    char known_id[256];
    int new_id_count = 0;

    if (fseek(current, 0, SEEK_SET) != 0) {
        return 0;
    }

    while (fscanf(current, "%255s", current_id) == 1) {
        if (known == NULL) {
            ++new_id_count;
            continue;
        }

        if (fseek(known, 0, SEEK_SET) != 0) {
            return new_id_count;
        }

        bool found_in_known = false;
        while (fscanf(known, "%255s", known_id) == 1) {
            if (strcmp(current_id, known_id) == 0) {
                found_in_known = true;
                break;
            }
        }
        if (!found_in_known) {
            ++new_id_count;
        }
    }

    return new_id_count;
}

int uidl_store_promote(const char *known_path, const char *pending_path) {
    if (known_path == NULL || pending_path == NULL) {
        return -1;
    }
    unlink(known_path);
    return rename(pending_path, known_path);
}
