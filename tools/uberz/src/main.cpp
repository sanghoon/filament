/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utils/Path.h>

#include <getopt/getopt.h>

#include <fstream>
#include <iostream>
#include <string>

#include <utils/FixedCapacityVector.h>

#include <uberz/WritableArchive.h>

using namespace std;
using namespace utils;
using namespace filament::uberz;

static std::string g_outputFile = "materials.uberz";
static bool g_quietMode = false;

static const char* USAGE = R"TXT(
UBERZ aggregates and compresses a set of filamat files into a single archive file. It includes
metadata that specifies the feature set that each material supports. By default, it generates
a file called "materials.uberz" but this can be customized with -o.

Usage:
    UBERZ [options] <src_name_0> <src_name_1> ...

For each src_name, UBERZ looks for "src_name.filamat" and "src_name.spec" in the current
working directory. If either of these files do not exist, an error is reported. Each
pair of filamat/spec files corresponds to a material in the generated archive.

For more information on the format of the spec file, see the gltfio README.

Options:
   --help, -h
       Print this message
   --license, -L
       Print copyright and license information
   --output=filename, -o filename
       Specify a custom filename.
    --quiet, -q
        Suppress console output
)TXT";

static void printUsage(const char* name) {
    std::string execName(Path(name).getName());
    const std::string from("UBERZ");
    std::string usage(USAGE);
    for (size_t pos = usage.find(from); pos != std::string::npos; pos = usage.find(from, pos)) {
        usage.replace(pos, from.length(), execName);
    }
    puts(usage.c_str());
}

static void license() {
    static const char *license[] = {
        #include "licenses/licenses.inc"
        nullptr
    };

    const char **p = &license[0];
    while (*p)
        std::cout << *p++ << std::endl;
}

static int handleArguments(int argc, char* argv[]) {
    static constexpr const char* OPTSTR = "hLqo:";
    static const struct option OPTIONS[] = {
            { "help",    no_argument,       0, 'h' },
            { "license", no_argument,       0, 'L' },
            { "quiet",   no_argument,       0, 'q' },
            { "output",  required_argument, 0, 'o' },
            { 0, 0, 0, 0 }  // termination of the option list
    };

    int opt;
    int optionIndex = 0;

    while ((opt = getopt_long(argc, argv, OPTSTR, OPTIONS, &optionIndex)) >= 0) {
        std::string arg(optarg ? optarg : "");
        switch (opt) {
            default:
            case 'h':
                printUsage(argv[0]);
                exit(0);
            case 'L':
                license();
                exit(0);
            case 'o':
                g_outputFile = optarg;
                break;
            case 'q':
                g_quietMode = true;
                break;
        }
    }

    return optind;
}

static size_t getFileSize(const char* filename) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
};

int main(int argc, char* argv[]) {
    const int optionIndex = handleArguments(argc, argv);
    const int numArgs = argc - optionIndex;
    if (numArgs < 1) {
        printUsage(argv[0]);
        return 1;
    }

    WritableArchive archive(argc - optionIndex);

    for (int argIndex = optionIndex; argIndex < argc; ++argIndex) {
        std::string name(argv[argIndex]);
        const Path filamatPath(name + ".filamat");
        if (!filamatPath.exists()) {
            cerr << "Unable to open " << filamatPath << endl;
            exit(1);
        }
        const Path specPath(name + ".spec");
        if (!specPath.exists()) {
            cerr << "Unable to open " << specPath << endl;
            exit(1);
        }

        const size_t filamatSize = getFileSize(filamatPath.c_str());
        FixedCapacityVector<uint8_t> filamatBuffer(filamatSize);
        std::ifstream in(filamatPath.c_str(), std::ifstream::in | std::ifstream::binary);
        if (!in.read((char*) filamatBuffer.data(), filamatSize)) {
            cerr << "Unable to consume " << filamatPath << endl;
            exit(1);
        }

        archive.addMaterial(name.c_str(), filamatBuffer.data(), filamatSize);

        std::string specLine;
        ifstream specStream(specPath.c_str());
        while (std::getline(specStream, specLine)) {
            archive.addSpecLine(specLine.c_str());
        }
    }

    FixedCapacityVector<uint8_t> binBuffer = archive.serialize();

    ofstream binStream(g_outputFile, ios::binary);
    if (!binStream) {
        cerr << "Unable to open " << g_outputFile << endl;
        exit(1);
    }
    binStream.write((const char*) binBuffer.data(), binBuffer.size());
    binStream.close();

    return 1; // TODO(prideout): DO NOT COMMIT
}
