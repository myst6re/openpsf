#include <libgen.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #define poll(fds, nfds, timeout) WSAPoll((fds), (nfds), (timeout))
#endif

#define DOMAIN "vlc-openpsf"

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_es.h>
#include <vlc_block.h>
#include <vlc_charset.h>
#include <vlc_url.h>
#include <vlc_meta.h>

#include <openpsf/openpsf_c.h>

struct demux_sys_t {
    PSF *handle;
    es_out_id_t *es;
    date_t pts;
    vlc_tick_t duration;
    uint64_t current_sample;
};

static int Open(vlc_object_t *obj);
static void Close(vlc_object_t *obj);
static int Demux(demux_t *demux);
static int Control(demux_t *demux, int query, va_list args);

vlc_module_begin()
    set_shortname("OpenPSF")
    set_description("PSF decoder VLC plugin using OpenPSF")
    set_capability("demux", 100)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_DEMUX)
    set_callbacks(Open, Close)
    add_bool("openpsf-legacy-reverb", false, "Use the old Pete's implementation of reverb",
             "Use the old Pete's implementation of reverb", false)
    add_bool("openpsf-legacy-reverb-upsampling", false, "Use the old Pete's implementation of reverb upsampling",
             "Use the old Pete's implementation of reverb upsampling", false)
vlc_module_end()

static int Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;

    const uint8_t *peek = NULL;
    if (vlc_stream_Peek(demux->s, &peek, 3) < 3) {
        return VLC_EGENERIC;
    }

    if (memcmp(peek, "PSF", 3) != 0 && !demux->obj.force) {
        return VLC_EGENERIC;
    }

    char *filepath = vlc_uri2path(demux->s->psz_url);

    if (filepath == NULL) {
        msg_Err(demux, "PSF needs a local filepath");
        return VLC_EGENERIC;
    }

    char *psz_filepath = ToLocaleDup(filepath);
    free(filepath);
    char *psz_filepath_copy = strdup(psz_filepath);

    const char *dir = dirname(psz_filepath_copy); // Dirname can modify filepath
    char he_bios_path[65535] = {};
    sprintf(he_bios_path, "%s/hebios.bin", dir);
    free(psz_filepath_copy);

    if (! openpsf_initialize_psx_core(he_bios_path)) {
        msg_Err(demux, "Cannot find a valid hebios.bin");
        free(psz_filepath);
        return VLC_EGENERIC;
    }

    bool legacy_reverb = var_InheritBool(demux, "openpsf-legacy-reverb");
    bool legacy_reverb_upsampling = var_InheritBool(demux, "openpsf-legacy-reverb-upsampling");

    int flags = (legacy_reverb ? OPENPSF_LEGACY_REVERB : 0) | (legacy_reverb_upsampling ? OPENPSF_LEGACY_REVERB_UPSAMPLING : 0);

    PSF *psf = openpsf_create_with_flags(flags, 0);

    if (!openpsf_open(psf, psz_filepath, false)) {
        openpsf_destroy(psf);
        msg_Err(demux, "Cannot open PSF file %s", psz_filepath);
        free(psz_filepath);
        return VLC_EGENERIC;
    }

    free(psz_filepath);

    demux_sys_t *sys = malloc(sizeof(demux_sys_t));
    if (unlikely(sys == NULL)) {
        openpsf_destroy(psf);
        return VLC_ENOMEM;
    }

    sys->handle = psf;
    sys->current_sample = 0;

    es_format_t fmt;
    es_format_Init(&fmt, AUDIO_ES, VLC_CODEC_S16N);
    fmt.audio.i_rate              = openpsf_get_sample_rate(psf);
    fmt.audio.i_channels          = openpsf_get_channel_count(psf);
    fmt.audio.i_physical_channels = AOUT_CHANS_STEREO;
    fmt.audio.i_bitspersample     = openpsf_get_bits_per_seconds(psf);
    fmt.i_bitrate                 = fmt.audio.i_rate * fmt.audio.i_channels * fmt.audio.i_bitspersample;

    sys->duration = CLOCK_FREQ * openpsf_get_sample_count(psf) / fmt.audio.i_rate;

    sys->es = es_out_Add(demux->out, &fmt);
    es_format_Clean(&fmt);

    if (sys->es == NULL) {
        free(sys);
        openpsf_destroy(psf);
        return VLC_ENOMEM;
    }

    date_Init(&sys->pts, openpsf_get_sample_rate(psf), 1);
    date_Set(&sys->pts, VLC_TICK_0);

    demux->p_sys      = sys;
    demux->pf_demux   = Demux;
    demux->pf_control = Control;

    msg_Info(demux, "OpenPSF decoder initialized\n%s", openpsf_get_last_status(psf));

    return VLC_SUCCESS;
}

static int Demux(demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    const unsigned samples_per_chunk = 1024;
    const size_t bytes_per_sample = sizeof(uint16_t) * openpsf_get_channel_count(sys->handle);
    const size_t buffer_size = samples_per_chunk * bytes_per_sample;

    block_t *block = block_Alloc(buffer_size);
    if (unlikely(block == NULL)) {
        return VLC_DEMUXER_EGENERIC;
    }

    int read_samples = openpsf_decode(sys->handle, (uint16_t *)block->p_buffer, samples_per_chunk);

    if (read_samples == 0) {
        block_Release(block);
        return VLC_DEMUXER_EOF;
    }

    if (read_samples < 0) {
        block_Release(block);
        return VLC_DEMUXER_EGENERIC;
    }

    sys->current_sample += (uint64_t)read_samples;

    if (date_Get(&sys->pts) >= VLC_TICK_0 + sys->duration) {
        block_Release(block);
        return VLC_DEMUXER_EOF;
    }

    block->i_buffer = (unsigned)read_samples * bytes_per_sample;

    block->i_pts = block->i_dts = date_Get(&sys->pts);
    block->i_length = date_Increment(&sys->pts, (unsigned)read_samples) - block->i_pts;

    es_out_SetPCR(demux->out, block->i_pts);
    es_out_Send(demux->out, sys->es, block);

    return VLC_DEMUXER_SUCCESS;
}

static int SeekToTime(demux_t *demux, vlc_tick_t target_time)
{
    demux_sys_t *sys = demux->p_sys;

    if (target_time < 0) {
        target_time = 0;
    }
    if (sys->duration > 0 && target_time > sys->duration) {
        target_time = sys->duration;
    }

    uint64_t sample_rate   = openpsf_get_sample_rate(sys->handle);
    uint64_t target_sample = (uint64_t)target_time * sample_rate / CLOCK_FREQ;
    uint64_t samples_to_skip = 0;

    if (target_sample >= sys->current_sample) {
        samples_to_skip = target_sample - sys->current_sample;
    } else {
        if (!openpsf_rewind(sys->handle)) {
            return VLC_EGENERIC;
        }
        sys->current_sample = 0;
        samples_to_skip = target_sample;
    }

    if (samples_to_skip > 0) {
        int skipped = openpsf_decode(sys->handle, NULL, (unsigned int)samples_to_skip);
        if (skipped < 0) {
            return VLC_EGENERIC;
        }
        sys->current_sample += (uint64_t)skipped;
    }

    date_Set(&sys->pts, VLC_TICK_0 + target_time);
    es_out_SetPCR(demux->out, date_Get(&sys->pts));

    return VLC_SUCCESS;
}

static int Control(demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query) {
        case DEMUX_CAN_SEEK:
            *va_arg(args, bool *) = true;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            *va_arg(args, vlc_tick_t *) = sys->duration;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg(args, vlc_tick_t *) = date_Get(&sys->pts) - VLC_TICK_0;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME: {
            vlc_tick_t target_time = va_arg(args, vlc_tick_t);
            return SeekToTime(demux, target_time);
        }

        case DEMUX_GET_POSITION: {
            double *pos = va_arg(args, double *);
            if (sys->duration > 0) {
                *pos = (double)(date_Get(&sys->pts) - VLC_TICK_0) / (double)sys->duration;
            } else {
                *pos = 0.0;
            }
            return VLC_SUCCESS;
        }

        case DEMUX_SET_POSITION: {
            double pos = va_arg(args, double);
            if (sys->duration <= 0) {
                return VLC_EGENERIC;
            }
            vlc_tick_t target_time = (vlc_tick_t)(pos * (double)sys->duration);
            return SeekToTime(demux, target_time);
        }

        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = true;
            return VLC_SUCCESS;

        case DEMUX_SET_PAUSE_STATE:
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) = VLC_TICK_FROM_MS(DEFAULT_PTS_DELAY);
            return VLC_SUCCESS;

        case DEMUX_GET_META: {
            vlc_meta_t *meta = va_arg(args, vlc_meta_t *);

            const char *title     = openpsf_get_tag(sys->handle, "title");
            const char *artist    = openpsf_get_tag(sys->handle, "artist");
            const char *game      = openpsf_get_tag(sys->handle, "game");
            const char *year      = openpsf_get_tag(sys->handle, "year");
            const char *genre     = openpsf_get_tag(sys->handle, "genre");
            const char *copyright = openpsf_get_tag(sys->handle, "copyright");
            const char *comment   = openpsf_get_tag(sys->handle, "comment");

            if (title && *title)         vlc_meta_SetTitle(meta, title);
            if (artist && *artist)       vlc_meta_SetArtist(meta, artist);
            if (game && *game)           vlc_meta_SetAlbum(meta, game);
            if (year && *year)           vlc_meta_SetDate(meta, year);
            if (genre && *genre)         vlc_meta_SetGenre(meta, genre);
            if (copyright && *copyright) vlc_meta_SetCopyright(meta, copyright);
            if (comment && *comment)     vlc_meta_SetDescription(meta, comment);

            return VLC_SUCCESS;
        }
        default:
            return VLC_EGENERIC;
    }
}

static void Close(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    if (sys != NULL) {
        if (sys->handle != NULL) {
            openpsf_close(sys->handle);
            openpsf_destroy(sys->handle);
        }

        if (sys->es != NULL) {
            es_out_Del(demux->out, sys->es);
        }
        free(sys);
    }
}
