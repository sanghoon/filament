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

#include <uberz/ArchiveCache.h>

#include <utils/memalign.h>

using namespace utils;

namespace filament::uberz {

static bool strIsEqual(const CString& a, const char* b) {
    return strncmp(a.c_str(), b, a.size()) == 0;
}

// This uberz file format involves zero parsing. Just copy the data blob into a struct and convert
// all offset fields into pointers.
void ArchiveCache::load(void* archiveData, uint64_t archiveByteCount) {
    assert_invariant(mArchive == nullptr);
    uint64_t* basePointer = (uint64_t*) utils::aligned_alloc(archiveByteCount, 8);
    memcpy(basePointer, archiveData, archiveByteCount);
    mArchive = (ReadableArchive*) basePointer;
    convertOffsetsToPointers(mArchive);
    mMaterials = FixedCapacityVector<Material*>(mArchive->specsCount);
}

// This loops though all ubershaders and returns the first one that meets the given requirements.
Material* ArchiveCache::getMaterial(const ArchiveRequirements& meshRequirements) {
    for (uint64_t i = 0; i < mArchive->specsCount; ++i) {
        const ArchiveSpec& spec = mArchive->specs[i];
        if (spec.blendingMode != meshRequirements.blendingMode) {
            continue;
        }
        if (spec.shadingModel != meshRequirements.shadingModel) {
            continue;
        }
        bool specIsSuitable = true;

        // For each feature required by the mesh, this ubershader is suitable only if it includes a
        // feature flag for it and the feature flag is either OPTIONAL or REQUIRED.
        for (const auto& req : meshRequirements.features) {
            const CString& meshRequirement = req.first;
            if (req.second == false) {
                continue;
            }
            bool found = false;
            for (uint64_t j = 0; j < spec.flagsCount && !found; ++j) {
                const ArchiveFlag& flag = spec.flags[j];
                if (strIsEqual(meshRequirement, flag.name)) {
                    if (flag.value != ArchiveFeature::UNSUPPORTED) {
                        found = true;
                    }
                    break;
                }
            }
            if (!found) {
                specIsSuitable = false;
                break;
            }
        }

        // If this ubershader requires a certain feature to be enabled in the glTF, but the glTF
        // mesh doesn't have it, then this ubershader is not suitable. This occurs very rarely, so
        // it intentionally comes after the other suitability check.
        for (uint64_t j = 0; j < spec.flagsCount && specIsSuitable; ++j) {
            ArchiveFlag& flag = spec.flags[j];
            if (UTILS_UNLIKELY(flag.value == ArchiveFeature::REQUIRED)) {
                // This allocates a new CString just to make a robin_map lookup, but this is rare
                // because almost none of our feature flags are REQUIRED.
                auto iter = meshRequirements.features.find(CString(flag.name));
                if (iter == meshRequirements.features.end() || iter.value() == false) {
                    specIsSuitable = false;
                }
            }
        }

        if (specIsSuitable) {
            if (mMaterials[i] == nullptr) {
                mMaterials[i] = Material::Builder()
                    .package(spec.package, spec.packageByteCount)
                    .build(mEngine);
            }
            return mMaterials[i];
        }
    }
    return nullptr;
}

void ArchiveCache::destroyMaterials() {
    for (auto mat : mMaterials) mEngine.destroy(mat);
    mMaterials.clear();
}

ArchiveCache::~ArchiveCache() {
    assert_invariant(mMaterials.size() == 0 &&
        "Please call destroyMaterials explicitly to ensure correct destruction order");
    free(mArchive);
}

} // namespace filament::uberz
