#include <std_compat/memory.h>
#include <map>
#include <string>

#include "libpressio_ext/cpp/data.h"
#include "libpressio_ext/cpp/compressor.h"
#include "libpressio_ext/cpp/options.h"
#include "libpressio_ext/cpp/pressio.h"
#include "pressio_options.h"
#include "pressio_data.h"
#include "pressio_compressor.h"
#include "cleanup.h"

#include "tools_common.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_image.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vpx_decoder.h"

#define ERR_STR(_KEY, _MSG) {_KEY, #_KEY ": " _MSG}

/**
 * TODO: Read through headers, see what minimum/preferred implementation requirements
 * are in current version
 * Determine which VPX layer to add plugin for (make VP8/9 selectable from metacompressor API?)
 */

namespace libpressio { namespace vpx {

/**
 * NOTE: Flags for vpx_..._flags_t types:
 * vpx_enc_frame_flags_t: passed to vpx_codec_encode
 *      VPX_EFLAG_FORCE_KF: Force this frame to be a keyframe
 * vpx_codec_er_flags_t : error resilience? LOOK UP
 *      ...
 * vpx_codec_flags_t: passed to codec_init
 */

const std::vector<std::string> PVPX_CODECS = {
    "vp8",
    "vp9"
};

// Long error strings from docs
const std::map<vpx_codec_err_t, std::string> PVPX_CODEC_ERR {
    ERR_STR(VPX_CODEC_OK,
        "Operation completed without error"),
    ERR_STR(VPX_CODEC_ERROR,
        "Unspecified error"),
    ERR_STR(VPX_CODEC_MEM_ERROR,
        "Memory operation failed"),
    ERR_STR(VPX_CODEC_ABI_MISMATCH,
        "ABI version mismatch"),
    ERR_STR(VPX_CODEC_INCAPABLE,
        "Algorithm does not have required capability"),
    ERR_STR(VPX_CODEC_UNSUP_BITSTREAM,
        "The given bitstream is not supported"),
    ERR_STR(VPX_CODEC_UNSUP_FEATURE,
        "Encoded bitstream uses an unsupported feature"),
    ERR_STR(VPX_CODEC_CORRUPT_FRAME,
        "The coded data for this stream is corrupt or incomplete"),
    ERR_STR(VPX_CODEC_INVALID_PARAM,
        "An application-supplied parameter is not valid"),
    ERR_STR(VPX_CODEC_LIST_END,
        "An iterator reached the end of list")
};

const std::map<std::string, vpx_img_fmt_t> PVPX_IMG_FMT {
    {"none", VPX_IMG_FMT_NONE},
    {"YV12", VPX_IMG_FMT_YV12},
    {"I420", VPX_IMG_FMT_I420},
    {"I422", VPX_IMG_FMT_I422},
    {"I444", VPX_IMG_FMT_I444},
    {"I440", VPX_IMG_FMT_I440},
    {"NV12", VPX_IMG_FMT_NV12},
    {"I420_16", VPX_IMG_FMT_I42016},
    {"I422_16", VPX_IMG_FMT_I42216},
    {"I444_16", VPX_IMG_FMT_I44416},
    {"I440_16", VPX_IMG_FMT_I44016}
};

const std::map<std::string, int> PVPX_DL {
    {"realtime", VPX_DL_REALTIME},
    {"good_quality", VPX_DL_GOOD_QUALITY},
    {"best_quality", VPX_DL_BEST_QUALITY}
};

template <class cfg_t>
class codec_ctx {
    vpx_codec_ctx_t ctx;
    vpx_codec_pts_t ctr;

    public:
    codec_ctx(const char* name, const std::string& type)
    {
        if (name == "encoder")
        {
            codec_ctx(name, )
        }
    }

    private:
    vpx_codec_iface_t get_iface(const std::string& name)
    {
        
    }
};

class vpx_plugin : public libpressio_compressor_plugin
{
    public:

    // Returns a totally new copy of the options struct?
    struct pressio_options get_options_impl() const override {
        pressio_options options;
        set(options, "vpx:codec", codec_name);
        set(options, "vpx:frame_fmt", frame_fmt);
        set(options, "vpx:enc_frame_flags", enc_flags);
        return options;
    }

    int set_options_impl(pressio_options const& options) override {
        bool preconf_change = false;
        get_preconf(options, "vpx:codec", &codec_name, preconf_change);
        get_preconf(options, "vpx:frame_fmt", &frame_fmt, preconf_change);
        get(options, "vpx:enc_frame_flags", &enc_flags);
        // Reinit context if config changed
        if (encode_is_init && preconf_change)
        {
            init_enc_ctx();
        } 
        return 0;
    }

    struct pressio_options get_configuration_impl() const override {
        struct pressio_options options;
        set(options, "vpx:codec", PVPX_CODECS);
        std::vector<std::string> fmt_opts;
        for (auto keypair : PVPX_IMG_FMT)
        {
            fmt_opts.push_back(keypair.first);
        }
        set(options, "vpx:frame_fmt", fmt_opts);
        set(options, "vpx:enc_frame_flags", "TODO");
        return options;
    }

    struct pressio_options get_documentation_impl() const override
    {
        struct pressio_options options;
        set(options, "vpx:codec", "codec implementation (either vp8 or vp9) to use");
        set(options, "vpx:frame_fmt", "raw color data format used by input/decoded frames");
        // ENCODER CFG STRUCT (LIMITED subset of cfg params)
        set(options, "vpx:nthreads", "Maximum number of threads to use internally");
        set(options, "vpx:bitstream_profile", "Bitstream profile for codec");
        // Bitdepth, timebase & framesize auto-configured
        set(options, "vpx:error_resilient_mode", "Enable error resillience modes");
        set(options, "vpx:multi_pass_mode", "Multi-pass encoding flags");
        // Lag frames and drop frames set to false - consider if enable option in future
        set(options, "vpx:rc_end_usage", "Rate control algorithm to use");
        set(options, "vpx:rc_resize_allowed", "Enable/disable spatial resampling");
        set(options, "vpx:rc_scale_factor", "Percentage of input frame size to scale to");
        
        set(options, "vpx:target_bitrate", "Target data rate");
        set(options, "vpx:best_quantizer", "Best quality quantizer");
        set(options, "vpx:worst_quantizer", "Worst quality quantizer");
        set(options, "vpx:keyframe_mode", "Keyframe placement mode");
        set(options, "vpx:keyframe_min", "Minimum keyframe interval (if automatic)");
        set(options, "vpx:keyframe:max", "Maximum keyframe interval (if automatic)");
        // Look into vizier modes, if you can find anything on them

        return options;
    }
    
    int compress_impl(const pressio_data* input, pressio_data* output) override
    {
        vpx_codec_err_t res = VPX_CODEC_OK;
        // Create encoder context on first call
        if (!encode_is_init)
        {
            init_enc_ctx();
        }
        // Frame or termination
        vpx_image_t* frame = NULL;
        // TODO: See about adding in-place buffer use.
        //       For now, copy internal framebuffer
        if (input)
        {
            // If passing, wrap input buffer as image
            size_t img_w = input->get_dimension(0);
            size_t img_h = input->get_dimension(1);
            uint8_t* src = reinterpret_cast<uint8_t*>(input->data());
            frame = vpx_img_wrap(NULL, PVPX_IMG_FMT.at(this->frame_fmt),
                                 img_w, img_h, 0, src);
            if (!frame)
            {
                return set_error(1, "pressio_data input invalid, "
                                    "could not format as frame");
            }
        }
        // Pass to compressor
        res = vpx_codec_encode(&this->encode_ctx, frame, encode_ctr, 1,
                               this->enc_flags, this->deadline);
        if (res != VPX_CODEC_OK)
        {
            return codec_error(res);
        }
        this->encode_ctr++;
        
        // Return buffer packets
        vpx_codec_iter_t iter = NULL;
        const vpx_codec_cx_pkt_t* enc_pkt;
        while (enc_pkt = vpx_codec_get_cx_data(&this->encode_ctx, &iter))
        {
            switch (enc_pkt->kind)
            {
                case VPX_CODEC_CX_FRAME_PKT:
                {
                    // TODO: See above, in-place buffer
                    *output = pressio_data::copy(pressio_byte_dtype, 
                        enc_pkt->data.frame.buf, {enc_pkt->data.frame.sz});
                }
                // TODO: Add in additional cases for metrics packets
            }
        }
        return res;
    }

    int decompress_impl(const struct pressio_data* input, struct pressio_data* output) override {
        vpx_codec_err_t res = VPX_CODEC_OK;
        // TODO: See about adding in-place buffer use.
        //       For now, copy internal framebuffer
        
        // Pass to decoder
        uint8_t* src = reinterpret_cast<uint8_t*>(input->data());
        res = vpx_codec_decode(&this->decode_ctx, src, input->size_in_bytes(),
                               NULL, 0);
        
        // Return buffer packets
        vpx_codec_iter_t iter = NULL;
        vpx_image_t* frame = vpx_codec_get_frame(&this->decode_ctx, &iter);
        *output = pressio_data::copy(pressio_byte_dtype,
            frame->img_data, {frame->w, frame->h, frame->bit_depth});
        
        if (res != VPX_CODEC_OK)
        {
            return codec_error(res);
        }
        return res;
    }

    // Version info
    const char* prefix() const override {
        return "vpx";
    }
    
    // This could be more dynamically handled I'm sure
    const char* version() const override {
        return vpx_codec_version_str();
    }

    int major_version() const override { return vpx_codec_version_major(); }

    int minor_version() const override { return vpx_codec_version_minor(); }

    int patch_version() const override { return vpx_codec_version_patch(); }

    std::shared_ptr<libpressio_compressor_plugin> clone() override {
        // Check up on what "make unique" does
        return compat::make_unique<vpx_plugin>(*this);
    }

    private:

    std::string codec_name { "vp8" };
    std::string frame_fmt  { "YV12" };
    vpx_enc_frame_flags_t enc_flags = 0;
    vpx_enc_deadline_t deadline = VPX_DL_REALTIME;

    bool encode_is_init = false;
    bool decode_is_init = false;

    vpx_codec_ctx_t encode_ctx;
    cleanup clean_enc_ctx = 
    vpx_codec_pts_t encode_ctr = 0;
    vpx_codec_ctx_t decode_ctx;
    cleanup clean_enc_ctx = 
    vpx_codec_pts_t decode_ctr = 0;

    int codec_error(vpx_codec_err_t rc)
    {
        return set_error(int(rc), PVPX_CODEC_ERR.at(rc));
    }

    template <class PointerType, class StringType>
    bool get_preconf(
            pressio_options const& options, StringType&& key, PointerType value, bool& update_flag) const
    {
        PointerType new_value;
        pressio_options_key_status status = get(options, key, value);
        update_flag |=
            (status == pressio_options_key_set && *value != *new_value)
        return update_flag;
    }

    void init_enc_ctx()
    {
        vpx_codec_iface_t* interface =
        get_vpx_encoder_by_name(codec_name.c_str())->codec_interface();
        // PLACEHOLDER
        vpx_codec_enc_cfg_t config;
        vpx_codec_enc_config_default(interface, &config, 0);
        vpx_codec_enc_init(&encode_ctx, interface, &config, 0);
        encode_is_init = true;
    }

    static vpx_fixed_buf_t vpx_wrap_buf(void* buf, size_t size_bytes)
    {
        return {buf, size_bytes};
    }
};

static pressio_register compressor_vpx_plugin(compressor_plugins(),
    "vpx", [](){return compat::make_unique<vpx_plugin>();});

}}