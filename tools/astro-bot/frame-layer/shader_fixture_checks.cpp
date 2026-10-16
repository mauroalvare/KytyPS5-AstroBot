// Synthetic file-loading checks only. These bytes are not GPU-valid shaders.
#include "layer.cpp"
#include <stdexcept>

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct FixtureDirectory {
    std::filesystem::path path;
    FixtureDirectory() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               ("astro-frame-synthetic-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(nonce));
        if (!std::filesystem::create_directory(path)) throw std::runtime_error("Temporary fixture collision");
    }
    ~FixtureDirectory() {
        // Every fixture is a direct child of our newly created directory.
        // No recursive removal or user-selected directory is involved.
        std::error_code error;
        for (std::filesystem::directory_iterator it(path, error), end; !error && it != end; it.increment(error)) {
            std::filesystem::remove(it->path(), error);
        }
        std::filesystem::remove(path, error);
    }
};

struct RestoreEnvironment {
    std::string previous;
    RestoreEnvironment() {
        if (const auto* value = std::getenv("ASTRO_FRAME_SHADER_DIR")) previous = value;
    }
    ~RestoreEnvironment() { _putenv_s("ASTRO_FRAME_SHADER_DIR", previous.c_str()); }
};
}

int main() try {
    FixtureDirectory temporary;
    RestoreEnvironment restore;
    _putenv_s("ASTRO_FRAME_SHADER_DIR", temporary.path.string().c_str());
    const std::vector<uint32_t> synthetic{0x07230203, 0x00010600, 0, 1, 7};
    const auto bytes = synthetic.size() * sizeof(uint32_t);
    const auto digest = ShaderFingerprint(synthetic.data(), bytes);
    const std::string key(64, 'a');
    int checks = 0;
    for (int variant = 0; variant < 6; ++variant) {
        auto payload = synthetic;
        if (variant == 1) payload.back() ^= 1; // Sidecar no longer matches.
        if (variant == 4) payload.front() = 0; // Matching digest, wrong magic.
        const auto filename = variant == 5 ? std::string("invalid-input-digest") : key;
        const auto file = temporary.path / (filename + ".spv");
        const auto checksum = temporary.path / (filename + ".sha256");
        const size_t length = variant == 3 ? bytes - 1 : bytes;
        {
            std::ofstream output(file, std::ios::binary);
            output.write(reinterpret_cast<const char*>(payload.data()), std::streamsize(length));
            Require(bool(output), "Could not create synthetic module fixture");
        }
        if (variant != 2) {
            std::ofstream output(checksum);
            output << (variant == 4 ? ShaderFingerprint(payload.data(), length) : digest) << '\n';
            Require(bool(output), "Could not create synthetic checksum fixture");
        }
        auto device = std::make_shared<Device>();
        LoadShaderOverrides(device);
        Require(device->shaderOverrides.size() == (variant == 0 ? 1u : 0u), "Synthetic fixture acceptance mismatch");
        if (variant == 0) Require(device->shaderOverrides.at(key) == synthetic, "Loaded fixture bytes changed");
        std::filesystem::remove(file);
        std::filesystem::remove(checksum);
        ++checks;
    }
    std::printf("%d synthetic shader-file checks passed; no game data, window or GPU work was used.\n", checks);
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAILED: %s\n", error.what());
    return 1;
}
