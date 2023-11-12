#include <argparse/argparse.hpp>
#include <boost/uuid/detail/sha1.hpp>
#include <iostream>

#include "GitCommands.hpp"

// TODO: remove when this bug is fixed and use .choices() instead
// https://github.com/p-ranav/argparse/issues/307
std::string verifyType(const std::string& type)
{
    if (type != "commit" && type != "blob" && type != "tree" && type != "tag") {
        GENERATE_EXCEPTION("Wrong type: {}, available types: "
                           "[commit, blob, tree, tag]",
                           type);
    }
    return type;
}

int main(int argc, char* argv[])
{
    argparse::ArgumentParser program("wygit");

    // clang-format off
    argparse::ArgumentParser initCommand("init");
    initCommand.add_description("Initialize a new, empty git repository.");
    initCommand.add_argument("path")
               .help("Path to directory where you want to create repository.")
               .metavar("directory")
               .nargs(argparse::nargs_pattern::optional)
               .default_value(".");
    
    argparse::ArgumentParser catFileCommand("cat-file");
    catFileCommand.add_description("Provide content of repository objects");
    catFileCommand.add_argument("type")
                  .help("Specify the type")
                  .metavar("type");
    catFileCommand.add_argument("object")
                  .help("The object to display")
                  .metavar("object");

    argparse::ArgumentParser hashObjectCommand("hash-object");
    hashObjectCommand.add_description("Compute object ID and optionally creates a blob from a file");
    hashObjectCommand.add_argument("path")
                     .help("Read object from file")
                     .metavar("path");
    hashObjectCommand.add_argument("-t")
                     .help("Specify the type")
                     .metavar("type")
                     .default_value("blob");
    hashObjectCommand.add_argument("-w")
                     .help("Actually write the object into the database")
                     .flag();
    argparse::ArgumentParser logCommand("log");
    logCommand.add_description("Display history of a given commit");
    logCommand.add_argument("commit")
               .help("Commit to start at")
               .metavar("commit")
               .default_value("HEAD");

    argparse::ArgumentParser lsTreeCommand("ls-tree");
    lsTreeCommand.add_description("Pretty-print a tree object");
    lsTreeCommand.add_argument("tree")
                 .help("A tree-ish object")
                 .metavar("tree");
    lsTreeCommand.add_argument("-r")
                 .help("Recurse into sub-trees")
                 .flag();
    
    argparse::ArgumentParser showRefCommand("show-ref");
    showRefCommand.add_description("List references");

    argparse::ArgumentParser tagCommand("tag");
    tagCommand.add_description("List and create tags");
    tagCommand.add_argument("name")
              .help("Name of the tag")
              .nargs(argparse::nargs_pattern::optional);
    tagCommand.add_argument("object")
              .help("The object the new tag will point to")
              .default_value("HEAD");
    tagCommand.add_argument("-a")
              .help("Whether to create a tag object")
              .flag();

    argparse::ArgumentParser revParseCommand("rev-parse");
    revParseCommand.add_description("Parse revision (or other objects) identifiers");
    revParseCommand.add_argument("name")
                   .help("The name to parse");

    argparse::ArgumentParser lsFilesCommand("ls-files");
    lsFilesCommand.add_description("List all the stage files");

    argparse::ArgumentParser commitCommand("commit");
    commitCommand.add_description("Record changes to the repository");
    commitCommand.add_argument("-m")
                 .help("Message to associate with this commit")
                 .metavar("message");

    program.add_subparser(initCommand);
    program.add_subparser(catFileCommand);
    program.add_subparser(hashObjectCommand);
    program.add_subparser(logCommand);
    program.add_subparser(lsTreeCommand);
    program.add_subparser(showRefCommand);
    program.add_subparser(tagCommand);
    program.add_subparser(revParseCommand);
    program.add_subparser(lsFilesCommand);
    program.add_subparser(commitCommand);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& myEx) {
        std::cerr << myEx.what() << std::endl << program;
        std::exit(EXIT_FAILURE);
    }
    // clang-format on

    try {
        if (program.is_subcommand_used("init")) {
            std::string pathToRepository =
                program.at<argparse::ArgumentParser>("init").get<std::string>(
                    "path");
            GitCommands::init(pathToRepository);
        }
        else if (program.is_subcommand_used("cat-file")) {
            auto& catFileSubParser =
                program.at<argparse::ArgumentParser>("cat-file");
            auto objectFormat =
                verifyType(catFileSubParser.get<std::string>("type"));

            auto objectToDisplay = catFileSubParser.get<std::string>("object");
            GitCommands::catFile(objectFormat, objectToDisplay);
        }
        else if (program.is_subcommand_used("hash-object")) {
            auto& hashObjectSubParser =
                program.at<argparse::ArgumentParser>("hash-object");
            auto pathToFile = hashObjectSubParser.get<std::string>("path");
            auto objectType =
                verifyType(hashObjectSubParser.get<std::string>("-t"));
            auto writeToFile = hashObjectSubParser.get<bool>("-w");
            std::cout << GitCommands::hashObject(pathToFile, objectType,
                                                 writeToFile)
                      << std::endl;
        }
        else if (program.is_subcommand_used("log")) {
            auto commit =
                program.at<argparse::ArgumentParser>("log").get<std::string>(
                    "commit");
            auto object = GitObject::findObject(commit);
            GitCommands::displayLog(object);
        }
        else if (program.is_subcommand_used("ls-tree")) {
            auto& lsTreeSubParser =
                program.at<argparse::ArgumentParser>("ls-tree");
            auto objectHash = GitObject::findObject(
                lsTreeSubParser.get<std::string>("tree"), "tree");
            auto recursive = lsTreeSubParser.get<bool>("-r");
            GitCommands::listTree(objectHash, "", recursive);
        }
        else if (program.is_subcommand_used("show-ref")) {
            auto references = GitRepository::repoDir("refs");
            for (const auto& [hash, refs] : GitCommands::getAll(references)) {
                for (const auto& ref : refs) {
                    std::cout << hash << ' ' << ref.string() << std::endl;
                }
            }
        }
        else if (program.is_subcommand_used("tag")) {
            auto& tagSubparser = program.at<argparse::ArgumentParser>("tag");
            bool isAssociative = tagSubparser.get<bool>("-a");
            bool tagHasName = tagSubparser.present("name").has_value();
            if (!isAssociative && !tagHasName) {
                auto tags = GitRepository::repoFile("refs", "tags");
                if (!tags.empty()) {
                    for (const auto& [_, tags] : GitCommands::getAll(tags)) {
                        for (const auto& tag : tags) {
                            std::cout << tag.filename().string() << std::endl;
                        }
                    }
                }
            }
            else if (tagHasName) {
                auto tagName = tagSubparser.get<std::string>("name");
                auto objectHash = GitObject::findObject(
                    tagSubparser.get<std::string>("object"));
                GitCommands::createTag(tagName, objectHash, isAssociative);
            }
            else {
                GENERATE_EXCEPTION("{}", tagSubparser.usage());
            }
        }
        else if (program.is_subcommand_used("rev-parse")) {
            auto& revSubParser =
                program.at<argparse::ArgumentParser>("rev-parse");
            auto objectName = revSubParser.get<std::string>("name");
            std::cout << GitObject::findObject(objectName, "") << std::endl;
        }
        else if (program.is_subcommand_used("ls-files")) {
            GitCommands::listFiles();
        }
        else if (program.is_subcommand_used("commit")) {
            auto& commitSubParser =
                program.at<argparse::ArgumentParser>("commit");
            std::string commitMessage =
                commitSubParser.present("-m")
                    ? commitSubParser.get<std::string>("-m")
                    : "Auto generated commit message";
            GitCommands::commit(commitMessage);
        }
        else {
            GENERATE_EXCEPTION("{}", program.help().str());
        }
    }
    catch (const std::exception& myEx) {
        std::cerr << myEx.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;

    // po::options_description desc("Allowed options");
    // // TODO: add more robust checks and meaningful description
    // // clang-format off
    // desc.add_options()
    //     ("help",
    //         "Produce help message"
    //     )
    //     ("init",
    //         po::value<std::string>()->implicit_value("."),
    //         "Create an empty Git repository"
    //     )
    //     ("cat-file",
    //         po::value<std::vector<std::string>>()->multitoken()->composing(),
    //         "Provide content of repository objects"
    //     )
    //     ("hash-file",
    //         po::value<std::vector<std::string>>()->multitoken()->composing(),
    //         "Compute object ID and optionally creates a blob from a file"
    //     )
    //     ("log",
    //         po::value<std::string>()->implicit_value("HEAD"),
    //         "Display history of a given commit."
    //     )
    //     ("ls-tree",
    //         po::value<std::vector<std::string>>()->multitoken()->composing(),
    //         "Pretty-print a tree object."
    //     )
    //     ("checkout",
    //         po::value<std::vector<std::string>>()->multitoken()->composing(),
    //         "Checkout a commit inside of a directory."
    //     )
    //     ("show-ref",
    //         po::value<std::string>()->implicit_value(".git/refs"),
    //         "List references."
    //     )
    //     // TODO: there is no ls-tag, make it part of tag somehow
    //     ("ls-tag",
    //         po::value<std::string>()->implicit_value(""),
    //         "List tags"
    //     )
    //     ("tag",
    //         po::value<std::vector<std::string>>()->multitoken()->composing(),
    //         "Create tag, use -a to create a tag object"
    //     )
    //     ("rev-parse",
    //         po::value<std::vector<std::string>>()->multitoken()->composing(),
    //         "Parse revision (or other objects )identifiers"
    //     )
    //     ("ls-files",
    //         po::value<std::string>()->implicit_value(""),
    //         "List all the stage files"
    //     )
    //     ("commit",
    //         po::value<std::string>()->implicit_value("."),
    //         "Create commit"
    //     )
    //     ("branch",
    //         po::value<std::string>()->implicit_value(""),
    //         "Create branch"
    //     );
    //     ;
    // // clang-format on

    // po::variables_map vm;
    // po::store(po::parse_command_line(argc, argv, desc), vm);
    // po::notify(vm);

    // if (vm.count("help") || argc < 2) {
    //     std::cout << desc << "\n";
    //     return 1;
    // }

    // try {
    //     if (vm.count("init")) {
    //         std::string pathToGitRepository = vm["init"].as<std::string>();
    //         GitCommands::init(pathToGitRepository);
    //     }
    //     else if (vm.count("cat-file")) {
    //         auto catFileArguments =
    //             vm["cat-file"].as<std::vector<std::string>>();
    //         if (catFileArguments.size() == 2) {
    //             GitCommands::catFile(catFileArguments[0],
    //             catFileArguments[1]);
    //         }
    //     }
    //     else if (vm.count("hash-file")) {
    //         auto hashFileArguments =
    //             vm["hash-file"].as<std::vector<std::string>>();
    //         if (hashFileArguments.size() == 2) {
    //             // TODO: make write optional argument
    //             GitCommands::hashFile(hashFileArguments[0],
    //                                   hashFileArguments[1], true);
    //         }
    //     }
    //     else if (vm.count("log")) {
    //         auto commitHash = vm["log"].as<std::string>();
    //         auto object = GitObject::findObject(commitHash);
    //         GitCommands::displayLog(object);
    //     }
    //     else if (vm.count("ls-tree")) {
    //         auto lsTreeArguments =
    //         vm["ls-tree"].as<std::vector<std::string>>(); auto objectHash =
    //         GitObject::findObject(lsTreeArguments[0], "tree"); auto recursive
    //         =
    //             lsTreeArguments.size() > 1 && lsTreeArguments[1] == "r";
    //         GitCommands::listTree(objectHash, "", recursive);
    //     }
    //     else if (vm.count("checkout")) {
    //         auto checkoutArguments =
    //             vm["checkout"].as<std::vector<std::string>>();
    //         if (checkoutArguments.size() == 1) {
    //             GitCommands::checkout(checkoutArguments[0]);
    //         }
    //     }
    //     else if (vm.count("show-ref")) {
    //         auto path = vm["show-ref"].as<std::string>();
    //         for (const auto& [hash, refs] :
    //         GitCommands::showReferences(path)) {
    //             for (const auto& ref : refs) {
    //                 std::cout << hash << '\t' << ref.string() << std::endl;
    //             }
    //         }
    //     }
    //     else if (vm.count("ls-tag")) {
    //         auto tagsPath = GitRepository::repoFile("refs", "tags");
    //         if (!tagsPath.empty()) {
    //             for (const auto& [_, tags] :
    //                  GitCommands::showReferences(tagsPath)) {
    //                 for (const auto& tag : tags) {
    //                     std::cout << tag.filename().string() << std::endl;
    //                 }
    //             }
    //         }
    //     }
    //     else if (vm.count("tag")) {
    //         auto tagArguments = vm["tag"].as<std::vector<std::string>>();
    //         if (tagArguments.size() >= 2) {
    //             auto tagName = tagArguments[0];
    //             auto objectHash = GitObject::findObject(tagArguments[1]);
    //             bool createAssociativeTag =
    //                 tagArguments.size() == 3 && tagArguments[2] == "a";
    //             GitCommands::createTag(tagName, objectHash,
    //                                    createAssociativeTag);
    //         }
    //     }
    //     else if (vm.count("rev-parse")) {
    //         auto revParseArgs =
    //         vm["rev-parse"].as<std::vector<std::string>>(); if
    //         (revParseArgs.size() >= 1) {
    //             auto objectName = revParseArgs[0];
    //             auto fmt = revParseArgs.size() > 1 ? revParseArgs[1] : "";
    //             std::cout << GitObject::findObject(objectName, fmt)
    //                       << std::endl;
    //         }
    //     }
    //     else if (vm.count("ls-files")) {
    //         GitCommands::listFiles();
    //     }
    //     else if (vm.count("commit")) {
    //         GitCommands::commit("Auto generated commit message");
    //     }
    //     else if (vm.count("branch")) {
    //         auto branchName = vm["branch"].as<std::string>();
    //         if (branchName.empty()) {
    //             GitCommands::showBranches();
    //         }
    //         else {
    //             GitCommands::createBranch(branchName);
    //         }
    //     }
    // }
    // catch (std::runtime_error myex) {
    //     std::cout << myex.what() << std::endl;
    // }
}
