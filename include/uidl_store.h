/**
 * @file uidl_store.h
 * @brief Persistence and diff for POP3 UIDL (unique-id listing) responses.
 *
 * The store treats each line of its on-disk files as a single unique ID.
 * A poll cycle writes the current server UIDL list to a "pending" file,
 * counts how many of its IDs are absent from the "known" file, and then
 * atomically promotes the pending file to become the new known file.
 */

#ifndef EMAILNOTIFIER_UIDL_STORE_H
#define EMAILNOTIFIER_UIDL_STORE_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Extract bare unique IDs from a raw POP3 UIDL response chunk.
 *
 * A UIDL response line has the form `<msg-number> <uid>\r\n`. This function
 * keeps only the unique-ID portion and writes one UID per line to @p out.
 * The function is streaming-friendly - it can be called repeatedly with
 * successive chunks of the same response.
 *
 * @param chunk Bytes received from the POP3 server (not NUL-terminated).
 * @param chunk_length Number of valid bytes in @p chunk.
 * @param out Open-for-write FILE* stream receiving the normalized IDs.
 */
void uidl_store_extract_ids(const char *chunk, size_t chunk_length, FILE *out);

/**
 * @brief Count how many IDs in @p current are not present in @p known.
 *
 * Both streams are rewound before reading. The function performs a
 * straightforward O(n*m) scan - acceptable because mailbox sizes are
 * small (hundreds of messages at most).
 *
 * @param known Known-IDs file; may be empty.
 * @param current Current-poll IDs file; must be non-NULL.
 * @return Number of IDs present in @p current but missing from @p known.
 */
int uidl_store_count_new(FILE *known, FILE *current);

/**
 * @brief Atomically replace @p known_path with @p pending_path.
 *
 * On POSIX, rename() is atomic across the same filesystem. The known file,
 * if present, is removed first so rename never fails with EEXIST on
 * systems that disallow overwrite.
 *
 * @param known_path Path of the authoritative known-IDs file.
 * @param pending_path Path of the pending file; consumed by the rename.
 * @return 0 on success, -1 on I/O error (errno is preserved).
 */
int uidl_store_promote(const char *known_path, const char *pending_path);

#ifdef __cplusplus
}
#endif

#endif /* EMAILNOTIFIER_UIDL_STORE_H */
