module;

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <list>
#include <memory>
#include <numeric>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

export module resurrect;

import cd.cd;
import cd.cdrom;
import cd.ecc;
import cd.edc;
import filesystem.iso9660;
import hash.sha1;
import readers.data_reader;
import readers.image_bin_reader;
import readers.image_iso_reader;
import utils.logger;
import utils.misc;



namespace gpsxre
{

typedef std::tuple<std::string, uint32_t, uint32_t, uint32_t> ContentEntry;


void write_sector(uint8_t *file_data, uint8_t *s, bool iso, uint32_t num_bytes)
{
    if(iso)
        std::copy_n(s, num_bytes, file_data);
    else
    {
        auto sector = (Sector *)s;
        if(sector->header.mode == 1)
        {
            std::copy_n(file_data, num_bytes, sector->mode1.user_data);
            Sector::ECC ecc = ECC().Generate((uint8_t *)&sector->header);
            std::copy_n(ecc.p_parity, sizeof(ecc.p_parity), sector->mode1.ecc.p_parity);
            std::copy_n(ecc.q_parity, sizeof(ecc.q_parity), sector->mode1.ecc.q_parity);
            sector->mode1.edc = EDC().update((uint8_t *)sector, offsetof(Sector, mode1.edc)).final();
        }
        else if(sector->header.mode == 2)
        {
            if(sector->mode2.xa.sub_header.submode & (uint8_t)CDXAMode::FORM2)
            {
                std::copy_n(file_data, num_bytes, sector->mode2.xa.form2.user_data);
                sector->mode2.xa.form2.edc = EDC().update((uint8_t *)&sector->mode2.xa.sub_header, offsetof(Sector, mode2.xa.form2.edc) - offsetof(Sector, mode2.xa.sub_header)).final();
            }
            else
            {
                std::copy_n(file_data, num_bytes, sector->mode2.xa.form1.user_data);
                sector->mode2.xa.form1.edc = EDC().update((uint8_t *)&sector->mode2.xa.sub_header, offsetof(Sector, mode2.xa.form1.edc) - offsetof(Sector, mode2.xa.sub_header)).final();
                Sector::ECC ecc = ECC().Generate(*sector, true);
                std::copy_n(ecc.p_parity, sizeof(ecc.p_parity), sector->mode2.xa.form1.ecc.p_parity);
                std::copy_n(ecc.q_parity, sizeof(ecc.q_parity), sector->mode2.xa.form1.ecc.q_parity);
            }
        }
    }
}


int calcify(std::filesystem::path skeleton_path, std::vector<ContentEntry> contents, std::unordered_map<std::string, std::string> hash_files, std::unordered_map<std::string, std::filesystem::path> file_hashes, bool iso)
{
    std::fstream skeleton(skeleton_path, std::fstream::in | std::fstream::out | std::fstream::binary);
    if(!skeleton.is_open())
    {
        LOG("failed to open: {}", skeleton_path.string());
        return -1;
    }

    for(auto const &c : contents)
    {
        try
        {
            auto file_path = file_hashes.at(hash_files.at(std::get<0>(c)));
            std::fstream file(file_path, std::fstream::in | std::fstream::binary);
            if(!file.is_open())
            {
                LOG("failed to open: {}", file_path.string());
                return -1;
            }
            std::vector<uint8_t> sector(iso ? FORM1_DATA_SIZE : CD_DATA_SIZE);
            std::vector<uint8_t> file_data(FORM1_DATA_SIZE);
            auto file_sectors = std::filesystem::file_size(file_path) / FORM1_DATA_SIZE;
            auto file_leftovers = std::filesystem::file_size(file_path) % FORM1_DATA_SIZE;
            uint64_t offset = std::get<1>(c);
            LOG("[{:08X}] writing {} from matching file: {}", offset, std::get<0>(c), file_path.string());
            for(int i = 0; i < file_sectors; ++i)
            {
                skeleton.seekg((std::streamoff)(offset * (iso ? FORM1_DATA_SIZE : CD_DATA_SIZE)), std::ios::beg);
                if(skeleton.fail())
                {
                    LOG("seek failed: {}", skeleton_path.string());
                    return -1;
                }

                skeleton.read((char *)sector.data(), sector.size());
                if(skeleton.fail())
                {
                    LOG("read failed: {}", skeleton_path.string());
                    return -1;
                }

                file.read((char *)file_data.data(), file_data.size());
                if(file.fail())
                {
                    LOG("read failed: {}", file_path.string());
                    return -1;
                }

                write_sector(file_data.data(), sector.data(), iso, FORM1_DATA_SIZE);

                skeleton.seekp((std::streamoff)(offset * (iso ? FORM1_DATA_SIZE : CD_DATA_SIZE)), std::ios::beg);
                if(skeleton.fail())
                {
                    LOG("seek failed: {}", skeleton_path.string());
                    return -1;
                }
                skeleton.write((char *)sector.data(), sector.size());
                if(skeleton.fail())
                {
                    LOG("write failed: {}", skeleton_path.string());
                    return -1;
                }
                offset++;
            }
            if(file_leftovers > 0)
            {
                skeleton.seekg((std::streamoff)(offset * (iso ? FORM1_DATA_SIZE : CD_DATA_SIZE)), std::ios::beg);
                if(skeleton.fail())
                {
                    LOG("seek failed: {}", skeleton_path.string());
                    return -1;
                }
                skeleton.read((char *)sector.data(), sector.size());
                if(skeleton.fail())
                {
                    LOG("read failed: {}", skeleton_path.string());
                    return -1;
                }

                file.read((char *)file_data.data(), file_leftovers);
                if(file.fail())
                {
                    LOG("read failed: {}", file_path.string());
                    return -1;
                }

                write_sector(file_data.data(), sector.data(), iso, file_leftovers);

                skeleton.seekp((std::streamoff)(offset * (iso ? FORM1_DATA_SIZE : CD_DATA_SIZE)), std::ios::beg);
                if(skeleton.fail())
                {
                    LOG("seek failed: {}", skeleton_path.string());
                    return -1;
                }
                skeleton.write((char *)sector.data(), sector.size());
            }
        }
        catch(const std::out_of_range& e)
        {
            if(std::get<0>(c) == "SYSTEM_AREA" && std::get<1>(c) == 0)
            {
                if(hash_files["5188431849b4613152fd7bdba6a3ff0a4fd6424b"] != "SYSTEM_AREA")
                    LOG("zeroing SYSTEM_AREA");
            }
            else if(std::get<3>(c) == 0 || hash_files["da39a3ee5e6b4b0d3255bfef95601890afd80709"] == std::get<0>(c))
                LOG("skipping zero-length file: {}", std::get<0>(c));
            else
                LOG("no matching file found, skipping: {}", std::get<0>(c));
        }
    }

    return 0;
}


std::vector<ContentEntry> analyze_contents(std::filesystem::path skeleton, uint32_t sectors_count, bool iso)
{
    std::unique_ptr<DataReader> data_reader;
    if(iso)
        data_reader = std::make_unique<Image_ISO_Reader>(skeleton.string());
    else
        data_reader = std::make_unique<Image_BIN_Reader>(skeleton.string());

    auto area_map = iso9660::area_map(data_reader.get(), sectors_count);
    if(area_map.empty())
    {
        LOG("error: no ISO9660 content found");
        return {};
    }

    std::vector<ContentEntry> contents;
    for(uint32_t i = 0; i + 1 < area_map.size(); ++i)
    {
        auto const &a = area_map[i];

        std::string name(a.name.empty() ? iso9660::area_type_to_string(a.type) : a.name);

        if(a.type == iso9660::Area::Type::SYSTEM_AREA || a.type == iso9660::Area::Type::FILE_EXTENT)
            contents.emplace_back(name, a.lba, scale_up(a.size, data_reader->sectorSize()), a.size);

        uint32_t gap_start = a.lba + scale_up(a.size, data_reader->sectorSize());
        if(gap_start < area_map[i + 1].lba)
        {
            uint32_t gap_size = area_map[i + 1].lba - gap_start;
            gap_size = gap_size > (sectors_count - gap_start) ? (sectors_count - gap_start) : gap_size;

            // >2048 sectors or >5% or more in relation to the total filesystem size
            if(gap_size > 2048 || (uint64_t)gap_size * 100 / sectors_count > 5)
                contents.emplace_back(std::format("GAP_{:07}", gap_start), gap_start, gap_size, gap_size * data_reader->sectorSize());
        }
    }

    return contents;
}


std::unordered_map<std::string, std::string> read_hash_hashes(std::filesystem::path hash_file)
{
    std::unordered_map<std::string, std::string> hashes;
    std::ifstream file(hash_file);
    
    if(!file)
    {
        LOG("error: cannot open hash file");
        return hashes;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        size_t pos = line.find(' ');
        if (pos != std::string::npos) {
            std::string hash = line.substr(0, pos);
            std::string path = line.substr(pos + 1);
            hashes[hash] = path;
        }
    }

    file.close();
    return hashes;
}


std::unordered_map<std::string, std::string> read_hash_files(std::filesystem::path hash_file)
{
    std::unordered_map<std::string, std::string> hashes;
    std::ifstream file(hash_file);
    
    if(!file)
    {
        LOG("error: cannot open hash file");
        return hashes;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        size_t pos = line.find(' ');
        if (pos != std::string::npos) {
            std::string hash = line.substr(0, pos);
            std::string path = line.substr(pos + 1);
            hashes[path] = hash;
        }
    }

    file.close();
    return hashes;
}


std::string CalculateSHA1(std::filesystem::path file_path)
{
    std::ifstream file(file_path, std::ios::binary);
    if(!file)
    {
        LOG("error opening file: {}", file_path.string());
        return "";
    }

    SHA1 sha1;
    std::vector<uint8_t> buffer(FORM1_DATA_SIZE);

    while(file)
    {
        file.read((char *)buffer.data(), FORM1_DATA_SIZE);
        std::streamsize bytes_read = file.gcount();
        if(bytes_read > 0)
            sha1.update(buffer.data(), bytes_read);
    }

    return sha1.final();
}


export int resurrect(std::filesystem::path file, bool force, bool recursive)
{
    int exit_code = 0;

    if(sizeof(std::streamoff) < 8)
        LOG("warning: compiled as 32bit, may not work with skeletons >4GB");

    auto skeleton = file.replace_extension(".skeleton");
    auto hash_file = file.replace_extension(".hash");
    auto dir = file.has_parent_path() ? file.parent_path() : std::filesystem::path(".");

    if(std::filesystem::exists(skeleton) && std::filesystem::exists(hash_file) && std::filesystem::is_directory(dir))
    {
        auto hash_hashes = read_hash_hashes(hash_file);
        auto hash_files = read_hash_files(hash_file);
        if(hash_hashes.empty())
        {
            LOG("error: invalid hash file");
            return -1;
        }

        std::unordered_map<std::string, std::filesystem::path> file_hashes;
        if(recursive)
        {
            LOG("hashing all files recursively in: {}", std::filesystem::absolute(dir).string());
            for(const auto &entry : std::filesystem::recursive_directory_iterator(dir))
            {
                if(entry.path() != skeleton && std::filesystem::is_regular_file(entry.path()))
                {
                    auto file_hash = CalculateSHA1(entry.path());
                    if(file_hash == "")
                        return -1;
                    file_hashes[file_hash] = entry.path();
                }
            }
        }
        else
        {
            LOG("hashing all files in: {}", std::filesystem::absolute(dir).string());
            for(const auto &entry : std::filesystem::directory_iterator(dir))
            {
                if(entry.path() != skeleton && std::filesystem::is_regular_file(entry.path()))
                {
                    auto file_hash = CalculateSHA1(entry.path());
                    if(file_hash == "")
                        return -1;
                    file_hashes[file_hash] = entry.path();
                }
            }
        }

        bool complete = true;
        for (const auto& [hash, path] : hash_hashes)
        {
            if (file_hashes.find(hash) == file_hashes.end() && (hash == "da39a3ee5e6b4b0d3255bfef95601890afd80709" || !(path == "SYSTEM_AREA" && hash == "5188431849b4613152fd7bdba6a3ff0a4fd6424b")))
            {
                LOG("no matching file for: {}", path);
                complete = false;
            }
        }
        
        if(!complete && !force)
        {
            LOG("failed: cannot resurrect skeleton, missing files (use --force to ignore)");
            exit_code = -1;
        }
        else
        {
            bool iso = false;
            auto dump_size = std::filesystem::file_size(skeleton);
            uint32_t sectors_count;
            if(dump_size % 2048 == 0)
            {
                if(dump_size % 2352 == 0)
                {
                    std::ifstream file(skeleton, std::ios::binary);
                    char buffer[12];
                    if (!file)
                    {
                        LOG("error: cannot open skeleton file");
                        return -1;
                    }
                    file.read(buffer, 12);
                    if(file.gcount() < 12)
                    {
                        LOG("error: failed to read skeleton file");
                        return -1;
                    }
                    if(std::memcmp(buffer, CD_DATA_SYNC, sizeof(CD_DATA_SYNC)))
                    {
                        iso = true;
                        sectors_count = dump_size / FORM1_DATA_SIZE;
                    }
                    else
                    {
                        iso = false;
                        sectors_count = dump_size / CD_DATA_SIZE;
                    }
                }
                else
                {
                    iso = true;
                    sectors_count = dump_size / FORM1_DATA_SIZE;
                }
            }
            else if(dump_size % 2352 == 0)
            {
                iso = false;
                sectors_count = dump_size / CD_DATA_SIZE;
            }
            else
            {
                LOG("error: invalid skeleton file size (is it compressed?)");
                return -1;
            }
            std::vector<ContentEntry> contents = analyze_contents(skeleton, sectors_count, iso);
            exit_code = calcify(skeleton, contents, hash_files, file_hashes, iso);
        }
    }
    else
    {
        LOG("error: skeleton/hash files not found");
        exit_code = -1;
    }

    return exit_code;
}

}
