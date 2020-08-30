//===--- ModuleDependencyScanner.h - Import Swift modules --------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/AST/ASTContext.h"
#include "swift/Frontend/ModuleInterfaceLoader.h"
#include "swift/Serialization/SerializedModuleLoader.h"

namespace swift {
    /// A module "loader" that looks for .swiftinterface and .swiftmodule files
    /// for the purpose of determining dependencies, but does not attempt to
    /// load the module files.
    class ModuleDependencyScanner : public SerializedModuleLoaderBase {
      /// The module we're scanning dependencies of.
      Identifier moduleName;

      /// Scan the given interface file to determine dependencies.
      llvm::ErrorOr<ModuleDependencies> scanInterfaceFile(
          Twine moduleInterfacePath, bool isFramework);

      InterfaceSubContextDelegate &astDelegate;
    public:
      Optional<ModuleDependencies> dependencies;

      /// Describes the kind of dependencies this scanner is able to identify
      ModuleDependenciesKind dependencyKind;

      ModuleDependencyScanner(
          ASTContext &ctx, ModuleLoadingMode LoadMode, Identifier moduleName,
          InterfaceSubContextDelegate &astDelegate,
          ModuleDependenciesKind dependencyKind = ModuleDependenciesKind::Swift)
          : SerializedModuleLoaderBase(ctx, nullptr, LoadMode,
                                       /*IgnoreSwiftSourceInfoFile=*/true),
            moduleName(moduleName), astDelegate(astDelegate),
            dependencyKind(dependencyKind) {}

      std::error_code findModuleFilesInDirectory(
          AccessPathElem ModuleID,
          const SerializedModuleBaseName &BaseName,
          SmallVectorImpl<char> *ModuleInterfacePath,
          std::unique_ptr<llvm::MemoryBuffer> *ModuleBuffer,
          std::unique_ptr<llvm::MemoryBuffer> *ModuleDocBuffer,
          std::unique_ptr<llvm::MemoryBuffer> *ModuleSourceInfoBuffer,
                                                 bool IsFramework) override;

      virtual void collectVisibleTopLevelModuleNames(
          SmallVectorImpl<Identifier> &names) const override {
        llvm_unreachable("Not used");
      }
    };

    /// A ModuleLoader that loads placeholder dependency module stubs specified in
    /// -placeholder-dependency-module-map-file
    /// This loader is used only in dependency scanning to inform the scanner that a
    /// set of modules constitute placeholder dependencies that are not visible to the
    /// scanner but will nevertheless be provided by the scanner's clients.
    /// This "loader" will not attempt to load any module files.
    class PlaceholderSwiftModuleScanner : public ModuleDependencyScanner {
      /// Scan the given placeholder module map
      void parsePlaceholderModuleMap(StringRef fileName) {
        ExplicitModuleMapParser parser(Allocator);
        auto result =
          parser.parseSwiftExplicitModuleMap(fileName, PlaceholderDependencyModuleMap);
        if (result == std::errc::invalid_argument) {
          Ctx.Diags.diagnose(SourceLoc(),
                             diag::placeholder_dependency_module_map_corrupted,
                             fileName);
        }
        else if (result == std::errc::no_such_file_or_directory) {
          Ctx.Diags.diagnose(SourceLoc(),
                             diag::placeholder_dependency_module_map_missing,
                             fileName);
        }
      }

      llvm::StringMap<ExplicitModuleInfo> PlaceholderDependencyModuleMap;
      llvm::BumpPtrAllocator Allocator;

    public:
      PlaceholderSwiftModuleScanner(ASTContext &ctx, ModuleLoadingMode LoadMode,
                                    Identifier moduleName,
                                    StringRef PlaceholderDependencyModuleMap,
                                    InterfaceSubContextDelegate &astDelegate)
          : ModuleDependencyScanner(ctx, LoadMode, moduleName, astDelegate,
                                    ModuleDependenciesKind::SwiftPlaceholder) {

        // FIXME: Find a better place for this map to live, to avoid
        // doing the parsing on every module.
        if (!PlaceholderDependencyModuleMap.empty()) {
          parsePlaceholderModuleMap(PlaceholderDependencyModuleMap);
        }
      }

      std::error_code findModuleFilesInDirectory(
          AccessPathElem ModuleID, const SerializedModuleBaseName &BaseName,
          SmallVectorImpl<char> *ModuleInterfacePath,
          std::unique_ptr<llvm::MemoryBuffer> *ModuleBuffer,
          std::unique_ptr<llvm::MemoryBuffer> *ModuleDocBuffer,
          std::unique_ptr<llvm::MemoryBuffer> *ModuleSourceInfoBuffer,
                                                 bool IsFramework) override;
    };
}
