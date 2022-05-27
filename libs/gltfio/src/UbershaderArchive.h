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

#ifndef GLTFIO_UBERSHADER_ARCHIVE_H
#define GLTFIO_UBERSHADER_ARCHIVE_H

#include <stdint.h>

#include <filament/Engine.h>
#include <filament/Material.h>
#include <filament/MaterialEnums.h>

#include <tsl/robin_map.h>

#include <utils/CString.h>
#include <utils/FixedCapacityVector.h>

namespace gltfio {

    struct Archive;
    struct ArchiveRequirements;

    class ArchiveCache {
    public:
        ArchiveCache(filament::Engine& engine) : mEngine(engine) {}
        ~ArchiveCache();

        void load(void* archiveData, uint64_t archiveByteCount);
        filament::Material* getMaterial(const ArchiveRequirements& requirements);
        filament::Material* getDefaultMaterial() { return mMaterials[0]; }
        const filament::Material* const* getMaterials() const noexcept { return mMaterials.data(); }
        size_t getMaterialsCount() const noexcept { return mMaterials.size(); }
        void destroyMaterials();

    private:
        filament::Engine& mEngine;
        utils::FixedCapacityVector<filament::Material*> mMaterials;
        Archive* mArchive = nullptr;
    };

    enum class ArchiveFeature : uint64_t {
        UNSUPPORTED,
        OPTIONAL,
        REQUIRED,
    };

    #pragma clang diagnostic push
    #pragma clang diagnostic warning "-Wpadded"

    struct ArchiveFlag {
        union {
            const char* name;
            uint64_t nameOffset;
        };
        ArchiveFeature value;
    };

    struct ArchiveSpec {
        union {
            filament::Shading shadingModel;
            uint32_t shadingModelValue;
        };
        union {
            filament::BlendingMode blendingMode;
            uint32_t blendingModeValue;
        };
        uint64_t flagsCount;
        union {
            ArchiveFlag* flags;
            uint64_t flagsOffset;
        };
        uint64_t packageByteCount;
        union {
            uint8_t* package;
            uint64_t packageOffset;
        };
    };

    struct Archive {
        uint32_t magic;
        uint32_t version;
        uint64_t specsCount;
        union {
            ArchiveSpec* specs;
            uint64_t specsOffset;
        };
    };

    #pragma clang diagnostic pop

    struct ArchiveRequirements {
        filament::Shading shadingModel;
        filament::BlendingMode blendingMode;
        tsl::robin_map<utils::CString, bool> features;
    };

} // namespace gltfio

#endif // GLTFIO_UBERSHADER_ARCHIVE_H
