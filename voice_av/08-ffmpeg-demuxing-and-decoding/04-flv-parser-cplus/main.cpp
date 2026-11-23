// Adapted from the 0Voice 07-04-flv_parser_cplus example.
// Read the FLV file one tag at a time and extract AVC and AAC without libavformat.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
uint32_t read_be(const uint8_t* data, std::size_t size)
{
    uint32_t value = 0;
    for (std::size_t i = 0; i < size; ++i)
        value = (value << 8) | data[i];
    return value;
}

void read_exact(std::istream& input, uint8_t* data, std::size_t size)
{
    if (size && !input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size)))
        throw std::runtime_error("Truncated FLV input");
}

void write_bytes(std::ostream& output, const uint8_t* data, std::size_t size)
{
    if (size && !output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size)))
        throw std::runtime_error("Could not write output");
}

struct AvcState
{
    unsigned nal_length_size = 0;
};

struct AacState
{
    unsigned object_type = 0;
    unsigned sample_rate_index = 0;
    unsigned channel_config = 0;
};

void write_annex_b(std::ostream& output, const uint8_t* data, std::size_t size)
{
    constexpr uint8_t start_code[] = {0, 0, 0, 1};
    write_bytes(output, start_code, sizeof(start_code));
    write_bytes(output, data, size);
}

void extract_avc(const std::vector<uint8_t>& tag, AvcState& avc, std::ostream& output)
{
    if (tag.empty() || (tag[0] & 0x0f) != 7)
        return;
    if (tag.size() < 5)
        throw std::runtime_error("Truncated AVC video tag");

    const uint8_t* data = tag.data() + 5;
    const std::size_t size = tag.size() - 5;
    if (tag[1] == 0)
    {
        // AVCDecoderConfigurationRecord (avcC) describes SPS, PPS and NAL lengths.
        if (size < 7 || data[0] != 1 || (data[4] & 3) == 2)
            throw std::runtime_error("Invalid AVC configuration");
        avc.nal_length_size = (data[4] & 3) + 1;
        std::size_t position = 6;
        const auto write_parameter_sets = [&](unsigned count) {
            for (unsigned i = 0; i < count; ++i)
            {
                if (size - position < 2)
                    throw std::runtime_error("Truncated AVC parameter set length");
                const auto length = read_be(data + position, 2);
                position += 2;
                if (length == 0 || length > size - position)
                    throw std::runtime_error("Invalid AVC parameter set length");
                write_annex_b(output, data + position, length);
                position += length;
            }
        };
        write_parameter_sets(data[5] & 0x1f);
        if (position == size)
            throw std::runtime_error("Missing AVC PPS count");
        const unsigned pps_count = data[position++];
        write_parameter_sets(pps_count);
    }
    else if (tag[1] == 1)
    {
        if (!avc.nal_length_size)
            throw std::runtime_error("Missing AVC configuration before video data");
        for (std::size_t position = 0; position < size;)
        {
            if (size - position < avc.nal_length_size)
                throw std::runtime_error("Truncated AVC NAL unit length");
            const auto length = read_be(data + position, avc.nal_length_size);
            position += avc.nal_length_size;
            if (length == 0 || length > size - position)
                throw std::runtime_error("Invalid AVC NAL unit length");
            write_annex_b(output, data + position, length);
            position += length;
        }
    }
}

void extract_aac(const std::vector<uint8_t>& tag, AacState& aac, std::ostream& output)
{
    if (tag.empty() || (tag[0] >> 4) != 10)
        return;
    if (tag.size() < 2)
        throw std::runtime_error("Truncated AAC audio tag");

    if (tag[1] == 0)
    {
        // Read the fixed opening fields of AudioSpecificConfig.
        if (tag.size() < 4)
            throw std::runtime_error("Truncated AAC configuration");
        const auto config = read_be(tag.data() + 2, 2);
        aac.object_type = config >> 11;
        aac.sample_rate_index = (config >> 7) & 15;
        aac.channel_config = (config >> 3) & 15;
        if (aac.object_type < 1 || aac.object_type > 4 || aac.sample_rate_index > 12 ||
            aac.channel_config == 0 || aac.channel_config > 7)
            throw std::runtime_error("AAC configuration is outside this ADTS example's scope");
    }
    else if (tag[1] == 1)
    {
        if (!aac.object_type)
            throw std::runtime_error("Missing AAC configuration before audio data");
        const auto length = tag.size() - 2 + 7;
        if (tag.size() == 2 || length > 0x1fff)
            throw std::runtime_error("Invalid ADTS frame length");

        // Seven byte MPEG-4 ADTS header with no CRC.
        const uint8_t header[] = {
            0xff,
            0xf1,
            static_cast<uint8_t>(((aac.object_type - 1) << 6) | (aac.sample_rate_index << 2) |
                                 (aac.channel_config >> 2)),
            static_cast<uint8_t>(((aac.channel_config & 3) << 6) | (length >> 11)),
            static_cast<uint8_t>((length >> 3) & 0xff),
            static_cast<uint8_t>(((length & 7) << 5) | 0x1f),
            0xfc,
        };
        write_bytes(output, header, sizeof(header));
        write_bytes(output, tag.data() + 2, tag.size() - 2);
    }
}

void print_metadata(const std::vector<uint8_t>& tag)
{
    // AMF0 short string "onMetaData", followed by an ECMA array or object.
    constexpr char name[] = "onMetaData";
    constexpr std::size_t name_length = sizeof(name) - 1;
    if (tag.size() < 4 + name_length || tag[0] != 2 || read_be(tag.data() + 1, 2) != name_length ||
        std::memcmp(tag.data() + 3, name, name_length) != 0)
        return;
    const uint8_t type = tag[3 + name_length];
    std::size_t position = 4 + name_length;
    if (type == 8 && tag.size() - position >= 4)
        position += 4;
    else if (type != 3)
        return;

    while (tag.size() - position >= 3)
    {
        const auto length = read_be(tag.data() + position, 2);
        position += 2;
        if (length >= tag.size() - position)
            return;
        const auto* key = reinterpret_cast<const char*>(tag.data() + position);
        position += length;
        const auto value_type = tag[position++];
        if (length == 0 && value_type == 9)
            return;
        if (value_type == 0 && tag.size() - position >= 8)
        {
            const uint64_t bits = (uint64_t{read_be(tag.data() + position, 4)} << 32) |
                                  read_be(tag.data() + position + 4, 4);
            double value;
            static_assert(sizeof(value) == sizeof(bits), "AMF0 numbers are binary64");
            std::memcpy(&value, &bits, sizeof(value));
            std::cout.write(key, length) << " = " << value << '\n';
            position += 8;
        }
        else if (value_type == 1 && position < tag.size())
        {
            std::cout.write(key, length) << " = " << (tag[position++] ? "true" : "false") << '\n';
        }
        else if (value_type == 2 && tag.size() - position >= 2)
        {
            const auto text_length = read_be(tag.data() + position, 2);
            position += 2;
            if (text_length > tag.size() - position)
                return;
            std::cout.write(key, length) << " = ";
            std::cout.write(reinterpret_cast<const char*>(tag.data() + position), text_length) << '\n';
            position += text_length;
        }
        else
        {
            // Nested objects and arrays are outside this scalar metadata display.
            return;
        }
    }
}

void parse_flv(std::istream& input, std::ostream& copy, std::ostream& video, std::ostream& audio)
{
    uint8_t header[9];
    read_exact(input, header, sizeof(header));
    const auto offset = read_be(header + 5, 4);
    if (std::memcmp(header, "FLV", 3) != 0 || header[3] != 1 || offset < sizeof(header))
        throw std::runtime_error("Invalid FLV header");
    write_bytes(copy, header, sizeof(header));
    std::array<uint8_t, 4096> extra_header;
    for (std::size_t remaining = offset - sizeof(header); remaining;)
    {
        const auto size = std::min(remaining, extra_header.size());
        read_exact(input, extra_header.data(), size);
        write_bytes(copy, extra_header.data(), size);
        remaining -= size;
    }
    uint8_t previous_size[4];
    read_exact(input, previous_size, sizeof(previous_size));
    if (read_be(previous_size, sizeof(previous_size)) != 0)
        throw std::runtime_error("Invalid first FLV previous-tag size");
    write_bytes(copy, previous_size, sizeof(previous_size));

    AvcState avc;
    AacState aac;
    std::vector<uint8_t> tag;
    uint64_t video_tags = 0, audio_tags = 0, metadata_tags = 0;
    uint32_t max_timestamp = 0;
    while (input.peek() != std::char_traits<char>::eof())
    {
        uint8_t tag_header[11];
        read_exact(input, tag_header, sizeof(tag_header));
        const auto size = read_be(tag_header + 1, 3);
        const auto timestamp = read_be(tag_header + 4, 3) | (uint32_t{tag_header[7]} << 24);
        max_timestamp = std::max(max_timestamp, timestamp);
        tag.resize(size);
        read_exact(input, tag.data(), tag.size());
        read_exact(input, previous_size, sizeof(previous_size));
        if (read_be(previous_size, sizeof(previous_size)) != sizeof(tag_header) + size)
            throw std::runtime_error("FLV previous-tag size does not match the tag");
        // Filtered (e.g. encrypted) tags are copied verbatim.
        if ((tag_header[0] & 0x20) == 0)
        {
            switch (tag_header[0] & 0x1f)
            {
                case 8:
                    ++audio_tags;
                    extract_aac(tag, aac, audio);
                    break;
                case 9:
                    ++video_tags;
                    extract_avc(tag, avc, video);
                    break;
                case 18:
                    ++metadata_tags;
                    print_metadata(tag);
                    break;
            }
        }
        write_bytes(copy, tag_header, sizeof(tag_header));
        write_bytes(copy, tag.data(), tag.size());
        write_bytes(copy, previous_size, sizeof(previous_size));
    }
    if (input.bad() || !input.eof())
        throw std::runtime_error("Could not read input");
    std::cout << "video tags: " << video_tags << ", audio tags: " << audio_tags
              << ", script tags: " << metadata_tags << ", max timestamp: " << max_timestamp
              << " ms, AVC NAL length bytes: " << avc.nal_length_size << '\n';
}
}  // namespace

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 5)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <input.flv> <copy.flv> [output.h264 output.aac]\n";
        return 1;
    }
    try
    {
        namespace fs = std::filesystem;
        const std::array<fs::path, 4> paths = {
            argv[1], argv[2], argc == 5 ? argv[3] : "parser.264", argc == 5 ? argv[4] : "parser.aac"};
        for (std::size_t i = 1; i < paths.size(); ++i)
        {
            for (std::size_t j = 0; j < i; ++j)
            {
                std::error_code error;
                if (fs::weakly_canonical(paths[i]) == fs::weakly_canonical(paths[j]) ||
                    fs::equivalent(paths[i], paths[j], error))
                    throw std::runtime_error("Input and output paths must be distinct");
            }
        }
        std::ifstream input(paths[0], std::ios::binary);
        if (!input)
            throw std::runtime_error("Could not open input FLV");
        std::ofstream copy(paths[1], std::ios::binary);
        std::ofstream video(paths[2], std::ios::binary);
        std::ofstream audio(paths[3], std::ios::binary);
        if (!copy || !video || !audio)
            throw std::runtime_error("Could not open output files");

        parse_flv(input, copy, video, audio);
        copy.close();
        video.close();
        audio.close();
        if (!copy || !video || !audio)
            throw std::runtime_error("Could not close output files");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
