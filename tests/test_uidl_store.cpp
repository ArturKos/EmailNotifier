/**
 * @file test_uidl_store.cpp
 * @brief Unit tests for the UIDL persistence/diff module.
 */

#include "uidl_store.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

/**
 * @brief Returns a unique temp file path that is removed at fixture teardown.
 */
class TempFile {
public:
    TempFile() {
        char path_template[] = "/tmp/uidl_store_test_XXXXXX";
        int fd = mkstemp(path_template);
        if (fd >= 0) {
            close(fd);
        }
        path_ = path_template;
    }

    ~TempFile() { std::filesystem::remove(path_); }

    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;

    const std::string &path() const { return path_; }

    void write(const std::string &content) const {
        std::ofstream stream(path_);
        stream << content;
    }

    std::string read() const {
        std::ifstream stream(path_);
        return std::string((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    }

private:
    std::string path_;
};

std::string extract(const std::string &raw) {
    TempFile out;
    FILE *stream = std::fopen(out.path().c_str(), "w");
    uidl_store_extract_ids(raw.data(), raw.size(), stream);
    std::fclose(stream);
    return out.read();
}

}  // namespace

TEST(UidlStoreExtractIds, StripsMessageNumberAndCarriageReturn) {
    const std::string raw =
        "1 abc-123\r\n"
        "2 def-456\r\n";
    EXPECT_EQ(extract(raw), "abc-123\ndef-456\n");
}

TEST(UidlStoreExtractIds, PreservesTrailingLineWithoutNewline) {
    EXPECT_EQ(extract("1 only-one"), "only-one");
}

TEST(UidlStoreExtractIds, IgnoresLeadingStatusLine) {
    // Real POP3 responses start with "+OK <count>\r\n". The extractor keeps
    // only the part after the first space on each line.
    const std::string raw =
        "+OK 3 messages\r\n"
        "1 uid-A\r\n"
        "2 uid-B\r\n"
        "3 uid-C\r\n";
    EXPECT_EQ(extract(raw), "3messages\nuid-A\nuid-B\nuid-C\n");
}

TEST(UidlStoreCountNew, ReturnsZeroWhenCurrentIsEmpty) {
    TempFile known;
    TempFile current;
    known.write("uid-A\nuid-B\n");
    current.write("");

    FILE *known_fp = std::fopen(known.path().c_str(), "rb");
    FILE *current_fp = std::fopen(current.path().c_str(), "rb");
    EXPECT_EQ(uidl_store_count_new(known_fp, current_fp), 0);
    std::fclose(known_fp);
    std::fclose(current_fp);
}

TEST(UidlStoreCountNew, TreatsAllAsNewWhenKnownIsNull) {
    TempFile current;
    current.write("uid-A\nuid-B\nuid-C\n");

    FILE *current_fp = std::fopen(current.path().c_str(), "rb");
    EXPECT_EQ(uidl_store_count_new(nullptr, current_fp), 3);
    std::fclose(current_fp);
}

TEST(UidlStoreCountNew, CountsOnlyIdsNotInKnown) {
    TempFile known;
    TempFile current;
    known.write("uid-A\nuid-B\n");
    current.write("uid-A\nuid-B\nuid-C\nuid-D\n");

    FILE *known_fp = std::fopen(known.path().c_str(), "rb");
    FILE *current_fp = std::fopen(current.path().c_str(), "rb");
    EXPECT_EQ(uidl_store_count_new(known_fp, current_fp), 2);
    std::fclose(known_fp);
    std::fclose(current_fp);
}

TEST(UidlStoreCountNew, HandlesIdenticalLists) {
    TempFile known;
    TempFile current;
    known.write("uid-A\nuid-B\n");
    current.write("uid-A\nuid-B\n");

    FILE *known_fp = std::fopen(known.path().c_str(), "rb");
    FILE *current_fp = std::fopen(current.path().c_str(), "rb");
    EXPECT_EQ(uidl_store_count_new(known_fp, current_fp), 0);
    std::fclose(known_fp);
    std::fclose(current_fp);
}

TEST(UidlStorePromote, ReplacesKnownWithPending) {
    TempFile known;
    TempFile pending;
    known.write("stale\n");
    pending.write("fresh-A\nfresh-B\n");

    ASSERT_EQ(uidl_store_promote(known.path().c_str(), pending.path().c_str()), 0);

    std::ifstream final_stream(known.path());
    std::string contents((std::istreambuf_iterator<char>(final_stream)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(contents, "fresh-A\nfresh-B\n");
    EXPECT_FALSE(std::filesystem::exists(pending.path()));
}

TEST(UidlStorePromote, SucceedsWhenKnownDoesNotExist) {
    TempFile pending;
    pending.write("first-run-A\n");
    const std::string known_path = "/tmp/uidl_store_promote_new_XXXXXX.uidl";
    std::filesystem::remove(known_path);

    ASSERT_EQ(uidl_store_promote(known_path.c_str(), pending.path().c_str()), 0);

    std::ifstream stream(known_path);
    std::string contents((std::istreambuf_iterator<char>(stream)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(contents, "first-run-A\n");
    std::filesystem::remove(known_path);
}
