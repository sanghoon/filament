# Description

`gltfio` is a loader library that consumes `gltf` or `glb` content and produces Filament
objects. For usage details, see `gltfio::AssetLoader`.

gltfio has two plug-in interfaces, `TextureProvider` and `MaterialProvider`.  Filament ships with
several ready-to-go implementations described below.

- `MaterialProvider` creates Filament materials in response to certain glTF requirements.
    - [UbershaderLoader](#ubershaderloader) loads pre-built materials.
        - [Ubershader Spec Files](#ubershader-spec-files)
        - [Ubershader Archive Files](#ubershader-archive-files)
    - `MaterialGenerator` builds materials at run time using the `filamat` library.
- `TextureProvider` creates and populates Filament `Texture` objects.
    - [StbProvider](#stbprovider) uses the STB library to read PNG and JPEG files.
    - [Ktx2Provider](#ktx2provider) uses the BasisU library to read KTX2 files.

# UbershaderLoader

`UbershaderLoader` is a ready-to-go implementation of the `MaterialProvider` interface that should
be used in applications that need fast startup times. There is no material compilation that
occurs at run time, but the shaders might be relatively large and complex.

At load time, the ubershader loader consumes an *ubershader archive* which is a precompiled set of
materials bundled with formal descriptions of the glTF features that they support.

The `uberz` command line tool consumes a list of `.spec` and `.filamat` files and produces a single
`.uberz` file.

## Ubershader Spec Files

An ubershader spec file is a simple text file with a `.spec` extension. It contains a list of
key-value pairs conforming to the following grammar. Each key-value pair is either a *feature flag*
or a *fundamental aspect*.

- Each feature flag can be **unsupported**, **required**, or **optional**.
- The fundamental aspect of the material cannot be changed, such as the blend mode.

```eBNF
spec = { [ comment | key_value_pair ] , "\n" } ;
comment = "#" , { any } ;
key_value_pair = ( fundamental_aspect | feature_flag ) ;
fundamental_aspect = ( blending | shading ) ;
feature_flag = identifier , equals , ("unsupported" | "required" | "optional") ;
blending = "BlendingMode" , equals ,
    ( "opaque" | "transparent" | "fade" | "add" | "masked" | "multiply" | "screen" ) ;
shading = "ShadingModel"  , equals ,
    ( "lit" | "subsurface" | "cloth" | "unlit" | "specularGlossiness") ;
equals = [ whitespace ] , "=" , [ whitespace ] ;
any = ? any character other than newline ? ;
whitespace = ? sequence of tabs and spaces ? ;
identifier = ? sequence of alphanumeric characters ? ;
```

The spec file **must** include all fundamental aspects, but it **may** include any set of feature
flags. If a feature flag is missing, it implicitly has the value of `unsupported`. For an up-to-date
list of recognized feature flags, look at the source for `UbershaderLoader::getMaterial`.

If a particular feature flag is set to `required` for a particular material, then the glTF loader
will bind that material to a given glTF mesh only if that feature is enabled in the mesh.

Usually, features are either `unsupported` or `optional`. For example, if the ubershader user can
set `normalIndex` in the material to `-1` to signal that they do not have a normal map, then normal
mapping should be specified as an `optional` feature of the ubershader.

## Ubershader Archive Files

An ubershader archive provides a way to bundle up a set of `filamat` files along with some metadata
that conveys which glTF features each material can handle. It is a file that has been compressed
with `zstd` and has an `.uberz` file extension. In uncompressed form, it has the following layout
(little endian is assumed).

```
[u32] magic identifier: UBER
[u32] version number for the archive format
[u64] number of specs
[u64] offset to SPECS
SPECS:
foreach spec {
    [u32] shading model
    [u32] blending model
    [u64] number of flags
    [u64] offset to FLAGLIST for this spec
    [u64] size in bytes of the filamat blob
    [u64] offset to FILAMAT for this spec
}
foreach spec {
    FLAGLIST:
    foreach flag {
        [u64] offset to FLAGNAME for this spec/flag pair
        [u64] flag value: 0 = unsupported, 1 = optional, or 2 = required
    }
}
foreach spec { foreach flag {
    FLAGNAME:
    [u8...] flag name, including null terminator
} }
foreach spec {
    FILAMAT:
    [u8...] filamat blob
}
```

In the above specification, each "offset" is a number of bytes between the top of the file to the
given label. These offsets are 64 bits so that they can be replaced with pointers in a C struct,
which allows the file to be consumed without any parsing. On 32-bit architectures, this still works
because we can simply ignore the unused padding after every pointer.
