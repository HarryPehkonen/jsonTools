// jtFrom — read + validate a file before it enters the pipeline.
#include <gtest/gtest.h>

#include "jt/errors.hpp"
#include "jt/from.hpp"

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

// Writes a temp file for the duration of one test.
class TempFile {
public:
    explicit TempFile(const std::string& contents) {
        std::vector<char> tmpl{'/', 't', 'm', 'p', '/', 'j', 't',
                               'X', 'X', 'X', 'X', 'X', 'X', '\0'};
        int fd = ::mkstemp(tmpl.data());
        if (fd == -1)
            std::abort();
        ::close(fd);
        path_ = tmpl.data();
        std::ofstream out(path_, std::ios::binary);
        out << contents;
    }
    ~TempFile() { std::remove(path_.c_str()); }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

TEST(JtFrom, ReadsAValidFile) {
    TempFile file(R"({"a":1,"b":[2,3]})");
    jt::FromResult result = jt::from_file(file.path(), {});
    EXPECT_EQ(result.doc.to_json(), R"({"a":1,"b":[2,3]})");
    EXPECT_TRUE(result.warnings.empty());
}

TEST(JtFrom, MissingFileIsAnError) {
    EXPECT_THROW(jt::from_file("/no/such/file.json", {}), jt::Error);
}

TEST(JtFrom, InvalidJsonIsAnError) {
    TempFile file("{not json");
    EXPECT_THROW(jt::from_file(file.path(), {}), jt::Error);
}

TEST(JtFrom, CheckOnlyStillValidates) {
    TempFile good(R"({"a":1})");
    jt::FromOptions opts;
    opts.check_only = true;
    EXPECT_NO_THROW(jt::from_file(good.path(), opts));

    TempFile bad("{");
    EXPECT_THROW(jt::from_file(bad.path(), opts), jt::Error);
}

TEST(JtFrom, MaxDepthRejectsTooDeepDocuments) {
    TempFile file(R"({"a":{"b":{"c":1}}})");
    jt::FromOptions opts;
    opts.max_depth = 2;
    EXPECT_THROW(jt::from_file(file.path(), opts), jt::Error);

    opts.max_depth = 3;
    EXPECT_NO_THROW(jt::from_file(file.path(), opts));
}

TEST(JtFrom, MaxSizeRejectsTooLargeFiles) {
    TempFile file(R"({"a":1,"b":2})");
    jt::FromOptions opts;
    opts.max_size = 4;
    EXPECT_THROW(jt::from_file(file.path(), opts), jt::Error);

    opts.max_size = 1024;
    EXPECT_NO_THROW(jt::from_file(file.path(), opts));
}

TEST(JtFrom, WarnDuplicatesReportsRepeatedKeys) {
    TempFile file(R"({"a":1,"a":2,"b":{"c":1,"c":2}})");
    jt::FromOptions opts;
    opts.warn_duplicates = true;
    jt::FromResult result = jt::from_file(file.path(), opts);
    ASSERT_EQ(result.warnings.size(), 2u);
    EXPECT_NE(result.warnings[0].find("/a"), std::string::npos);
    EXPECT_NE(result.warnings[1].find("/b/c"), std::string::npos);
    // Last value wins, as JSOM's parser does.
    EXPECT_EQ(result.doc.to_json(), R"({"a":2,"b":{"c":2}})");
}

TEST(JtFrom, DuplicateKeysAreSilentWithoutTheFlag) {
    TempFile file(R"({"a":1,"a":2})");
    jt::FromResult result = jt::from_file(file.path(), {});
    EXPECT_TRUE(result.warnings.empty());
}

// A key containing a '/' or '~' must be reported with its RFC 6901 escape.
TEST(JtFrom, DuplicateWarningEscapesPointerSegments) {
    TempFile file(R"({"a/b":1,"a/b":2})");
    jt::FromOptions opts;
    opts.warn_duplicates = true;
    jt::FromResult result = jt::from_file(file.path(), opts);
    ASSERT_EQ(result.warnings.size(), 1u);
    EXPECT_NE(result.warnings[0].find("/a~1b"), std::string::npos);
}

TEST(JtFrom, StringsContainingBracesDoNotConfuseTheDuplicateScanner) {
    TempFile file(R"({"a":"{\"a\":1,\"a\":2}","b":2})");
    jt::FromOptions opts;
    opts.warn_duplicates = true;
    jt::FromResult result = jt::from_file(file.path(), opts);
    EXPECT_TRUE(result.warnings.empty());
}

} // namespace
