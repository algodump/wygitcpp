#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <gtest/gtest.h>

#include "../CommandArguments.hpp"

std::filesystem::path REPO_PATH = std::filesystem::current_path() / "gitTest";

class GitCommandsTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        cleanRoot();
        GitCommands::init(REPO_PATH);
        std::filesystem::current_path(REPO_PATH);
    }

  private:
    void cleanRoot() { std::filesystem::remove_all(REPO_PATH); }
};

TEST_F(GitCommandsTest, GitInit)
{
    EXPECT_TRUE(std::filesystem::exists(REPO_PATH));
    EXPECT_TRUE(std::filesystem::exists(REPO_PATH / ".git"));

    auto pathToConfig = REPO_PATH / ".git" / "config";
    EXPECT_TRUE(std::filesystem::exists(pathToConfig));

    boost::property_tree::ptree config;
    boost::property_tree::read_ini(pathToConfig, config);
    ASSERT_EQ(config.get<int>("core.repositoryformatversion"), 0);
}

TEST_F(GitCommandsTest, CreateTree)
{
    auto fileOne = "file1.txt";
    auto fileTwo = "file2.txt";
    auto fileThree = "file3.txt";
    auto dirOne = REPO_PATH / "dir1";
    auto fileFour = "file4.txt";

    Utilities::writeToFile(fileOne, "one");
    Utilities::writeToFile(fileTwo, "two");
    Utilities::writeToFile(fileThree, "three");

    std::filesystem::create_directories(dirOne);
    Utilities::writeToFile(dirOne / fileFour, "four");

    auto treeHash = GitCommands::createTree(REPO_PATH);
    auto maybeTree = GitObjectFactory::read(treeHash);
    ASSERT_EQ(maybeTree->format(), "tree");

    GitTree* treeObject = static_cast<GitTree*>(maybeTree.get());
    const auto& leaves = treeObject->tree();
    ASSERT_EQ(leaves.size(), 4);

    auto findLeaf = [&](const std::string& filePath) {
        return std::find_if(
            leaves.begin(), leaves.end(),
            [&](const GitTreeLeaf& leaf) { return filePath == leaf.filePath; });
    };

    auto getMode = [&](const std::string& filePath) {
        auto leaf = findLeaf(filePath);
        return leaf != leaves.end() ? (*leaf).fileMode : "";
    };

    auto fileOneBlob = findLeaf(fileOne);
    auto fileTwoBlob = findLeaf(fileTwo);
    auto fileThreeBlob = findLeaf(fileThree);
    auto dirOneTree = findLeaf(dirOne);

    EXPECT_TRUE(fileOneBlob != leaves.end());
    EXPECT_TRUE(fileTwoBlob != leaves.end());
    EXPECT_TRUE(fileThreeBlob != leaves.end());
    EXPECT_TRUE(dirOneTree != leaves.end());

    std::string blobFileMode = "100644";
    ASSERT_EQ((*fileOneBlob).fileMode, blobFileMode);
    ASSERT_EQ((*fileTwoBlob).fileMode, blobFileMode);
    ASSERT_EQ((*fileThreeBlob).fileMode, blobFileMode);

    std::string treeFileMode = "040000";
    ASSERT_EQ((*dirOneTree).fileMode, treeFileMode);
}

TEST_F(GitCommandsTest, HashFileBlob)
{
    auto textFile = REPO_PATH / "test.txt";
    Utilities::writeToFile(textFile, "text");
    auto fileHash = GitCommands::hashFile(textFile, "blob");
    auto fileHashFromFS = GitObject::findObject(fileHash.data(), "blob");
    EXPECT_EQ(fileHash.data(), fileHashFromFS.data());
}

TEST(GitUtility, FileMode)
{
    using namespace std::filesystem;
    std::string fileModeDir = "fileModeDir";
    remove_all(fileModeDir);
    create_directory(fileModeDir);
    current_path(fileModeDir);

    // test executables and regular file
    {
        path fileOwx = "file1.txt";
        path fileGx = "file2.txt";
        path fileOthx = "file3.txt";
        path regularFile = "file4.txt";

        Utilities::writeToFile(fileOwx, "1");
        Utilities::writeToFile(fileGx, "2");
        Utilities::writeToFile(fileOthx, "3");
        Utilities::writeToFile(regularFile, "4");

        permissions(fileOwx, perms::owner_exec, perm_options::add);
        permissions(fileGx, perms::group_exec, perm_options::add);
        permissions(fileOthx, perms::others_exec, perm_options::add);

        ASSERT_EQ(GitTree::fileMode(directory_entry(fileOwx), "blob"),
                  "100755");
        ASSERT_EQ(GitTree::fileMode(directory_entry(fileGx), "blob"), "100755");
        ASSERT_EQ(GitTree::fileMode(directory_entry(fileOthx), "blob"),
                  "100755");
        ASSERT_EQ(GitTree::fileMode(directory_entry(regularFile), "blob"),
                  "100644");
    }

    // test directories
    {
        path testDirectory = "test";
        create_directory(testDirectory);
        ASSERT_EQ(GitTree::fileMode(directory_entry(testDirectory), "tree"),
                  "040000");
    }

    // wrong format
    {
        path yetAnotherFile = "fileWithWrongFormat.txt";
        Utilities::writeToFile(yetAnotherFile, "not a dir");
        try {
            GitTree::fileMode(directory_entry(yetAnotherFile), "tree");
            FAIL() << "Expected std::runtime_error" << std::endl;
        }
        catch (std::runtime_error& error) {
        }
    }

     // symlink
    {
        path target = "target";
        path link = "target_lk";
        Utilities::writeToFile(target, "target");
        create_symlink(target, link);
        ASSERT_EQ(GitTree::fileMode(directory_entry(link), "blob"), "120000");
    }
}

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}