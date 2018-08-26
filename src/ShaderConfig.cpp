#include <fstream>
using std::ifstream;
#include <sstream>
using std::stringstream;
#include <algorithm>
using std::sort;
#include <iostream>
using std::cout;
using std::endl;
#include <string>
using std::string;
using std::to_string;
#include <set>
using std::set;
#include <vector>
using std::vector;
#include <cctype> // isalpha
#include <stdexcept>
using std::runtime_error;

#include "ShaderConfig.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
namespace rj = rapidjson;

// TODO throw for unknown configuration options
// TODO check for valid glsl names for buffers and uniforms
// TODO fail if options are given more than once


static const string WINDOW_SIZE_OPTION("window_size");
static const string EASY_SHADER_MODE_OPTION("easy");
static const string ADVANCED_SHADER_MODE_OPTION("advanced");

static AudioOptions parse_audio_options(rj::Document& user_conf) {
    AudioOptions ao;

    rj::Value& audio_options = user_conf["audio_options"];
    if (!audio_options.IsObject())
        throw runtime_error("Audio options must be a json object");

    if (audio_options.HasMember("fft_smooth")) {
        rj::Value& fft_smooth = audio_options["fft_smooth"];
        if (!fft_smooth.IsNumber())
            throw runtime_error("fft_smooth must be a number between in the interval [0, 1]");
        ao.fft_smooth = fft_smooth.GetFloat();
        if (ao.fft_smooth < 0 || ao.fft_smooth > 1)
            throw runtime_error("fft_smooth must be in the interval [0, 1]");
    }
    if (audio_options.HasMember("wave_smooth")) {
        rj::Value& wave_smooth = audio_options["wave_smooth"];
        if (!wave_smooth.IsNumber())
            throw runtime_error("wave_smooth must be a number between in the interval [0, 1]");
        ao.wave_smooth = wave_smooth.GetFloat();
        if (ao.wave_smooth < 0 || ao.wave_smooth > 1)
            throw runtime_error("wave_smooth must be in the interval [0, 1]");
    }
    if (audio_options.HasMember("fft_sync")) {
        rj::Value& fft_sync = audio_options["fft_sync"];
        if (!fft_sync.IsBool())
            throw runtime_error("fft_sync must be true or false");
        ao.fft_sync = fft_sync.GetBool();
    }
    if (audio_options.HasMember("xcorr_sync")) {
        rj::Value& xcorr_sync = audio_options["xcorr_sync"];
        if (!xcorr_sync.IsBool())
            throw runtime_error("xcorr_sync must be true or false");
        ao.xcorr_sync = xcorr_sync.GetBool();
    }

    return ao;
}

static Buffer parse_image_buffer(rj::Document& user_conf) {
    Buffer image_buffer;
    image_buffer.name = "image";

    rj::Value& image = user_conf["image"];
    if (!image.IsObject())
        throw runtime_error("image is not a json object");
    if (!image.HasMember("geom_iters"))
        throw runtime_error("image does not contain the geom_iters option");

    rj::Value& geom_iters = image["geom_iters"];

    if (!geom_iters.IsInt() || geom_iters.GetInt() <= 0)
        throw runtime_error("image.geom_iters must be a positive integer");
    image_buffer.geom_iters = geom_iters.GetInt();

    if (image.HasMember("clear_color")) {
        rj::Value& clear_color = image["clear_color"];
        if (!(clear_color.IsArray() && clear_color.Size() == 3))
            throw runtime_error("image.clear_color must be an array of 3 real numbers each between 0 and 1");
        for (int i = 0; i < 3; ++i) {
            if (clear_color[i].IsNumber())
                image_buffer.clear_color[i] = clear_color[i].GetFloat();
            else
                throw runtime_error("image.clear_color must be an array of 3 real numbers each between 0 and 1");
        }
    }

    return image_buffer;
}

static Buffer parse_buffer(rj::Value& buffer, string buffer_name, set<string>& buffer_names) {
    Buffer b;

    b.name = buffer_name;
    if (b.name == string(""))
        throw runtime_error("Buffer must have a name");
    if (buffer_names.count(b.name))
        throw runtime_error("Buffer name " + b.name + " already used (buffers must have unique names)");
    buffer_names.insert(b.name);
    if (!(std::isalpha(b.name[0]) || b.name[0] == '_'))
        throw runtime_error("Invalid buffer name: " + b.name + " buffer names must start with either a letter or an underscore");
    if (b.name == string("image"))
        throw runtime_error("Cannot name buffer image");

    if (!buffer.IsObject())
        throw runtime_error("Buffer " + b.name + " is not a json object");

    if (buffer.HasMember("clear_color")) {
        rj::Value& b_clear_color = buffer["clear_color"];
        if (!b_clear_color.IsArray() || b_clear_color.Size() != 3) {
            throw runtime_error(b.name + ".clear_color must be an array of 3 real numbers each between 0 and 1");
        }
        for (int i = 0; i < 3; ++i) {
            if (b_clear_color[i].IsNumber())
                b.clear_color[i] = b_clear_color[i].GetFloat();
            else
                throw runtime_error(b.name + ".clear_color must be an array of 3 real numbers each between 0 and 1");
            // else if (b_clear_color[i].IsInt())
            //	b.clear_color[i] = b_clear_color[i].GetFloat()/256.f;
        }
    }


    if (!buffer.HasMember("size")) {
        cout << "Buffer: " << b.name << " does not have a size option, assuming the size of the buffer to be the window size" << endl;
    }
    else {
        rj::Value& b_size = buffer["size"];
        if (b_size.IsArray() && b_size.Size() == 2) {
            if (!b_size[0].IsInt() || !b_size[1].IsInt())
                throw runtime_error(b.name + ".size must be an array of two positive integers");
            b.width = b_size[0].GetInt();
            b.height = b_size[1].GetInt();
            b.is_window_size = false;
        }
        else if (!b_size.IsString() || b_size.GetString() != WINDOW_SIZE_OPTION) {
            throw runtime_error(b.name + ".size must be an array of two positive integers");
        }
    }

    if (!buffer.HasMember("geom_iters")) {
        cout << "Buffer: " << b.name << " does not have a geom_iters option, assuming the number of times to execute geometry shader is 1" << endl;
    }
    else {
        rj::Value& b_geom_iters = buffer["geom_iters"];
        if (!b_geom_iters.IsInt() || b_geom_iters.GetInt() == 0)
            throw runtime_error(b.name + ".geom_iters must be a positive integer");
        b.geom_iters = b_geom_iters.GetInt();
    }

    return b;
}

static vector<Buffer> parse_buffers(rj::Document& user_conf) {
    vector<Buffer> buffers_vec;

    rj::Value& buffers = user_conf["buffers"];
    if (!buffers.IsObject())
        throw runtime_error("buffers is not a json object");

    if (buffers.MemberCount() == 0) {
        return {};
    }

    // Catch buffers with the same name
    set<string> buffer_names;

    for (auto memb = buffers.MemberBegin(); memb != buffers.MemberEnd(); memb++) {
        buffers_vec.push_back(parse_buffer(memb->value, memb->name.GetString(), buffer_names));
    }

    return buffers_vec;
}

static vector<int> parse_render_order(rj::Document& user_conf, vector<Buffer>& buffers) {
    vector<int> ro;

    if (!user_conf.HasMember("render_order")) {
        for (int i = 0; i < buffers.size(); ++i)
            ro.push_back(i);
        return ro;
    }

    if (!buffers.size()) {
        return {};
    }

    rj::Value& render_order = user_conf["render_order"];
    if (!render_order.IsArray() || render_order.Size() == 0)
        throw runtime_error("render_order must be an array of strings (buffer names) with length > 0");
    for (unsigned int i = 0; i < render_order.Size(); ++i) {
        if (!render_order[i].IsString())
            throw runtime_error("render_order can only contain strings (buffer names)");

        string b_name = render_order[i].GetString();
        int index = -1;
        for (int j = 0; j < buffers.size(); ++j) {
            if (buffers[j].name == b_name) {
                index = j;
                break;
            }
        }
        if (index == -1)
            throw runtime_error("render_order member \"" + b_name + "\" must be the name of a buffer in \"buffers\"");

        // mRender_order contains indices into mBuffers
        ro.push_back(index);
    }

    return ro;
}

static void delete_unused_buffers(vector<Buffer>& buffers, vector<int>& render_order) {
    // Only keep the buffers that are used to render
    set<int> used_buff_indices;
    vector<Buffer> used_buffs;
    for (int i = 0; i < render_order.size(); i++) {
        if (!used_buff_indices.count(render_order[i])) {
            used_buffs.push_back(buffers[render_order[i]]);
            used_buff_indices.insert(render_order[i]);
        }
    }
    buffers = used_buffs;
}

Uniform parse_uniform(rj::Value& uniform, string uniform_name, set<string>& uniform_names) {
    Uniform u;

    if (uniform_names.count(uniform_name))
        throw runtime_error("Uniform name " + uniform_name + " already used (uniforms must have unique names)");
    uniform_names.insert(uniform_name);
    u.name = uniform_name;

    if (uniform.IsArray()) {
        if (uniform.Size() > 4)
            throw runtime_error("Uniform " + u.name + " must have dimension less than or equal to 4");
        for (unsigned int i = 0; i < uniform.Size(); ++i) {
            if (!uniform[i].IsNumber())
                throw runtime_error("Uniform " + u.name + " contains a non-numeric value.");
            u.values.push_back(uniform[i].GetFloat());
        }
    }
    else if (uniform.IsNumber()) {
        u.values.push_back(uniform.GetFloat());
    }
    else {
        throw runtime_error("Uniform " + u.name + " must be either a number or an array of numbers.");
    }
    return u;
}

static vector<Uniform> parse_uniforms(rj::Document& user_conf) {
    vector<Uniform> uniform_vec;

    rj::Value& uniforms = user_conf["uniforms"];
    if (!uniforms.IsObject())
        throw runtime_error("Uniforms must be a json object.");

    if (uniforms.MemberCount() == 0) {
        return {};
    }

    // Catch uniforms with the same name
    set<string> uniform_names;

    for (auto memb = uniforms.MemberBegin(); memb != uniforms.MemberEnd(); memb++) {
        uniform_vec.push_back(parse_uniform(memb->value, memb->name.GetString(), uniform_names));
    }

    return uniform_vec;
}

void ShaderConfig::parse_config_from_string(const std::string & json_str) {
    rj::Document user_conf;
    rj::ParseResult ok =
        user_conf.Parse<rj::kParseCommentsFlag | rj::kParseTrailingCommasFlag>(json_str.c_str());
    if (!ok)
        throw runtime_error("JSON parse error: " + string(rj::GetParseError_En(ok.Code())) +
                            " At char offset (" + to_string(ok.Offset()) + ")");

    if (!user_conf.IsObject())
        throw runtime_error("Invalid json file");

    if (user_conf.HasMember("initial_window_size")) {
        rj::Value& window_size = user_conf["initial_window_size"];
        if (!(window_size.IsArray() && window_size.Size() == 2 && window_size[0].IsInt() && window_size[1].IsInt()))
            throw runtime_error("initial_window_size must be an array of 2 positive integers");
        mInitWinSize.width = window_size[0].GetInt();
        mInitWinSize.height = window_size[1].GetInt();
    }

    if (user_conf.HasMember("audio_enabled")) {
        rj::Value& audio_enabled = user_conf["audio_enabled"];
        if (!audio_enabled.IsBool())
            throw runtime_error("audio_enabled must be true or false");
        mAudio_enabled = audio_enabled.GetBool();
    }

    if (user_conf.HasMember("audio_options")) {
        mAudio_ops = parse_audio_options(user_conf);
    }

    if (user_conf.HasMember("shader_mode")) {
        if (!user_conf["shader_mode"].IsString())
            throw runtime_error("shader_mode must be either \"easy\" or \"advanced\"");
        if (user_conf["shader_mode"].GetString() == EASY_SHADER_MODE_OPTION) {
            mSimpleMode = true;
        }
        else if (user_conf["shader_mode"].GetString() == ADVANCED_SHADER_MODE_OPTION) {
            mSimpleMode = false;
        }
        else {
            throw runtime_error("shader_mode must be either \"easy\" or \"advanced\"");
        }
    }
    else {
        mSimpleMode = true;
    }

    if (user_conf.HasMember("blend")) {
        if (!user_conf["blend"].IsBool())
            throw runtime_error("blend must be true or false");
        mBlend = user_conf["blend"].GetBool();
    }

    if (user_conf.HasMember("image")) {
        mImage = parse_image_buffer(user_conf);
    }
    else {
        mImage.name = "image";
    }

    if (user_conf.HasMember("buffers")) {
        mBuffers = parse_buffers(user_conf);
        mRender_order = parse_render_order(user_conf, mBuffers);
        delete_unused_buffers(mBuffers, mRender_order);
    }

    if (user_conf.HasMember("uniforms")) {
        mUniforms = parse_uniforms(user_conf);
    }
}

void ShaderConfig::parse_simple_config(const filesys::path & shader_folder) {
    // ignore mBuffers, mRender_order, and mImage options that have been parsed from json
    if (mBuffers.size() != 0) {
        cout << "Warning: ignoring \"buffers\" because shader_mode is set to " << EASY_SHADER_MODE_OPTION << endl;
    }
    mBuffers.clear();
    mRender_order.clear();
    mImage = Buffer{};

    // parse file list to decide if frag files should have custom or default geometry shader
    vector<string> frag_filenames;
    for (auto & p : filesys::directory_iterator(shader_folder)) {
        if (!filesys::is_regular_file(p))
            continue;

        const filesys::path &path = p.path();
        if (path.stem().string() == "image")
            continue;
        if (path.extension().string() == ".frag")
            frag_filenames.push_back(path.stem().string());
    }

    // simple mode renders in alphabetical order
    auto compare_lowercase = [](string l, string r) {
        for (char& c : l) c = tolower(c);
        for (char& c : r) c = tolower(c);
        return l < r;
    };
    sort(frag_filenames.begin(), frag_filenames.end(), compare_lowercase);

    mBlend = false;

    mImage.name = "image";
    for (auto& buffer_name : frag_filenames) {
        mRender_order.push_back(mBuffers.size());
        Buffer b;
        b.name = buffer_name;
        mBuffers.push_back(b);
    }
}

ShaderConfig::ShaderConfig(const filesys::path& shader_folder, const filesys::path& conf_file_path) {
    if (filesys::exists(conf_file_path)) {
        ifstream file(conf_file_path.string());
        stringstream str;
        str << file.rdbuf();
        parse_config_from_string(str.str());
    }
    else {
        mSimpleMode = true;
    }

    if (mSimpleMode) {
        cout << "shader_mode set to simple" << endl;
        parse_simple_config(shader_folder);
    }

    if (filesys::exists(filesys::path(shader_folder / (mImage.name + ".geom")))) {
        mImage.uses_default_geometry_shader = false;
    }
    else if (mImage.geom_iters > 1) {
        cout << "Warning: Buffer " << mImage.name << " is using default geometry shader and has geom_iters > 1 set. Performance could suffer." << endl;
    }
    for (Buffer& b : mBuffers) {
        if (filesys::exists(filesys::path(shader_folder / (b.name + ".geom")))) {
            b.uses_default_geometry_shader = false;
        }
        else if (b.geom_iters > 1) {
            cout << "Warning: Buffer " << b.name << " is using default geometry shader and has geom_iters > 1 set. Performance could suffer." << endl;
        }
    }
}

ShaderConfig::ShaderConfig(const string& json_str) {
    parse_config_from_string(json_str);
}
