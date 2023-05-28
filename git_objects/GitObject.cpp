#include "GitObject.hpp"

#include <fstream>
#include <memory>

#include "../utilities/Zlib.hpp"

namespace Git {

GitRepository GitObject::repository() const { return m_repository; }

GitObject::~GitObject() {}

GitHash GitObject::write(GitObject* gitObject, bool acutallyWrite)
{
    auto objectData = gitObject->serialize();
    auto fileContent = gitObject->format() + " " +
                       std::to_string(objectData.data().size()) + '\0' +
                       objectData.data();

    auto fileHash = SHA1::computeHash(fileContent);

    if (acutallyWrite) {
        auto objectFile = GitRepository::repoFile(
            gitObject->repository(), GitRepository::CreateDir::YES, "objects",
            fileHash.directoryName(), fileHash.fileName());
        Zlib::compress(objectFile, fileContent);
    }
    return fileHash;
}

std::filesystem::path GitObject::findObject(const GitRepository& repo,
                                            const std::string& name,
                                            const std::string& fmt, bool follow)
{
    return name;
}

GitCommit::GitCommit(const GitRepository& repository, const ObjectData& data)
    : GitObject(repository, data)
{
}

ObjectData GitCommit::serialize()
{
    std::ostringstream oss;

    oss << "tree"
        << " " << m_commitMessage.objectType << std::endl;
    oss << "parent"
        << " " << m_commitMessage.parent << std::endl;
    oss << "author"
        << " " << m_commitMessage.author << std::endl;
    oss << "commiter"
        << " " << m_commitMessage.commiter << std::endl;
    oss << "gpgsig"
        << " " << m_commitMessage.gpgsig << std::endl;
    oss << std::endl;
    oss << m_commitMessage.messaage << std::endl;

    return ObjectData(oss.str());
}

void GitCommit::deserialize(const ObjectData& data)
{
    m_commitMessage = parseCommitMessage(data.data());
}

std::string GitCommit::format() const { return "commit"; }

const CommitMessage& GitCommit::message() const { return m_commitMessage; }

CommitMessage GitCommit::parseCommitMessage(const std::string& data)
{
    std::istringstream iss(data);

    std::unordered_map<std::string, std::string> commitMessage;

    auto trim = [](const std::string& str) {
        return str.substr(str.find_first_not_of(" \n"));
    };

    for (std::string currentLine; std::getline(iss, currentLine);) {
        if (!currentLine.empty()) {
            auto keyEnds = currentLine.find_first_of(' ');
            if (commitMessage.find("gpgsig") != commitMessage.end()) {
                commitMessage["gpgsig"] += trim(currentLine);

                // PGP ends with:  -----END PGP SIGNATURE-----
                // if we reach the end of PGP, than break the loop
                // and parse the message that follows the signature.
                if (currentLine.find("END") == std::string::npos) {
                    continue;
                }
                else {
                    break;
                }
            }

            auto key = trim(currentLine.substr(0, keyEnds));
            auto value = trim(currentLine.substr(keyEnds));
            commitMessage[key] = value;
        }
    }

    // TODO: probably could be done in one loop
    for (std::string currentLine; std::getline(iss, currentLine);) {
        if (!currentLine.empty()) {
            commitMessage["message"] += currentLine;
        }
    }

    return {.objectType = commitMessage.at("tree"),
            .parent = commitMessage.at("parent"),
            .author = commitMessage.at("author"),
            .commiter = commitMessage.at("commiter"),
            .gpgsig = commitMessage.at("gpsig"),
            .messaage = commitMessage.at("message")};
}

GitTree::GitTree(const GitRepository& repository, const ObjectData& data)
    : GitObject(repository, data)
{
}

ObjectData GitTree::serialize()
{
    std::string data;
    
    for (const auto& leaf : m_tree) {
        data += leaf.fileMode;
        data += ' ';
        data += leaf.filePath;
        data += '\0';
        data += GitHash::encodeStringHash(leaf.hash);
    }
    return ObjectData(data);
}

std::vector<GitTreeLeaf> GitTree::parseGitTree(const std::string& data)
{
    std::vector<GitTreeLeaf> tree;

    auto maxPos = data.size();
    size_t start = 0;

    while (start < maxPos) {
        auto fileModeEnds = data.find(' ', start);
        auto fileMode = data.substr(start, fileModeEnds);
        assert(fileMode.size() == 5 || fileMode.size() == 6);

        auto pathEnds = data.find('\0', fileModeEnds);
        auto path = data.substr(fileModeEnds + 1, pathEnds);

        auto sha = GitHash::decodeBinaryHash(
            data.substr(pathEnds + 1, GitHash::BINARY_HASH_SIZE));

        start = GitHash::BINARY_HASH_SIZE + 1;
        tree.push_back(
            {.fileMode = fileMode, .filePath = path, .hash = GitHash(sha)});
    }
    return tree;
}

void GitTree::deserialize(const ObjectData& data)
{
    m_tree = parseGitTree(data.data());
}

std::string GitTree::format() const { return "tree"; }

const std::vector<GitTreeLeaf> GitTree::tree() const { return m_tree; }

GitTag::GitTag(const GitRepository& repository, const ObjectData& data)
    : GitObject(repository, data)
{
}

ObjectData GitTag::serialize() { return {}; }

void GitTag::deserialize(const ObjectData& data) {}

std::string GitTag::format() const { return "tag"; }

GitBlob::GitBlob(const GitRepository& repository, const ObjectData& data)
    : GitObject(repository, data)
{
}

ObjectData GitBlob::serialize() { return m_data; }

void GitBlob::deserialize(const ObjectData& data) { m_data = data; }

std::string GitBlob::format() const { return "blob"; }
} // namespace Git
