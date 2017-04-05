/* Copyright 2013-2017 Bas van den Berg
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CGenerator/MakefileGenerator.h"
#include "AST/Component.h"
#include "Utils/StringBuilder.h"
#include "Utils/TargetInfo.h"
#include "FileUtils/FileUtils.h"
#include "Utils/StringList.h"

using namespace C2;

MakefileGenerator::MakefileGenerator(const Component& component_,
                                     const std::string& libDir_,
                                     bool singleFile_,
                                     const TargetInfo& targetInfo_)
    : component(component_)
    , libDir(libDir_)
    , target(component.getName())
    , targetInfo(targetInfo_)
    , singleFile(singleFile_)
{}

void MakefileGenerator::write(const std::string& path) {
    StringList files;
    if (singleFile) {
        files.push_back(component.getName());
    } else {
        const ModuleList& mods = component.getModules();
        for (unsigned m=0; m<mods.size(); m++) {
            files.push_back(mods[m]->getName());
        }
    }

    std::string targetname = "../";
    std::string libname;
    switch (component.getType()) {
    case Component::EXECUTABLE:
        targetname += target;
        break;
    case Component::SHARED_LIB:
#ifdef __APPLE__
        targetname += "lib" + target + ".dylib";
        libname = "lib" + target + ".dylib";
#else
        targetname += "lib" + target + ".so";
        libname = "lib" + target + ".so";
#endif
        break;
    case Component::STATIC_LIB:
        targetname += "lib" + target + ".a";
        break;
    }

    std::string args = "-O3 -Wall -Wextra -Wno-unused -Wno-unused-parameter -std=c99";

    if (component.isSharedLib()) args += " -fPIC";

    StringBuilder out;
    out << "# WARNING: this file is auto-generated by the C2 compiler.\n";
    out << "# Any changes you make might be lost!\n\n";

    // all target
    out << "all: " << targetname << '\n';
    out << '\n';

    // our target
    out << targetname << ':';
    for (StringListConstIter iter=files.begin(); iter!=files.end(); ++iter) {
        out << ' ' << *iter << ".c";
    }
    out << '\n';

    // compile step
    for (StringListConstIter iter=files.begin(); iter!=files.end(); ++iter) {
        out << "\tgcc " << args << " -c " << *iter << ".c -o " << *iter << ".o\n";
    }
    const Component::Dependencies& deps = component.getDeps();
    // link step
    switch (component.getType()) {
    case Component::EXECUTABLE:
        out << "\tgcc -o " << targetname;
        for (StringListConstIter iter=files.begin(); iter!=files.end(); ++iter) {
            out << ' ' << *iter << ".o";
        }
        // TODO move after switch?
        for (unsigned i=0; i<deps.size(); i++) {
            const Component* dep = deps[i];
            // TEMP add -L flag if static lib (system static libs not yet supported)
            if (dep->isStaticLib()) {
                out << " -L" << libDir << '/' << dep->getName() << '/' << Str(targetInfo);
            }
            if (dep->getLinkName() != "") {
                out << " -l" << dep->getLinkName();
            }
        }
        break;
    case Component::SHARED_LIB:
        out << "#link against with: gcc main.c -L. -l<libname> -o test\n";
        out << "\tgcc";
        for (StringListConstIter iter=files.begin(); iter!=files.end(); ++iter) {
            out << ' ' << *iter << ".o";
        }
#ifdef __APPLE__
        out << " -dynamiclib -Wl,-headerpad_max_install_names -o " << targetname;
        out << " -Wl,-install_name," << libname;
        // TODO export script replacement?
#else
        out << " -shared -o " << targetname << " -Wl,-soname," << libname << ".1";
        out << " -Wl,--version-script=exports.version";
#endif
        break;
    case Component::STATIC_LIB:
        out << "\tar rcs " << targetname;
        for (StringListConstIter iter=files.begin(); iter!=files.end(); ++iter) {
            out << ' ' << *iter << ".o";
        }
        break;
    }
    out << '\n';
    out << '\n';

    // show target (for export debug)
    out << "symbols:\n";
#ifdef __APPLE__
    out << "\t nm -U " << targetname << '\n';
#else
    out << "\t nm -g -D -C --defined-only " << targetname << '\n';
#endif
    out << '\n';

    // clean target
    out << "clean:\n";
    out << "\t rm -f *.o *.a " << targetname << '\n';
    out << '\n';

    FileUtils::writeFile(path.c_str(), path + "/Makefile", out);
}
