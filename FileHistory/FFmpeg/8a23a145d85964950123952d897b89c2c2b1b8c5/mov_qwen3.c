Do not include anything else.

static int mov_read_udta_string(MOVContext *c, AVIOContext *pb, MOVAtom atom)
{
    char tmp_key[AV_FOURCC_MAX_STRING_SIZE] = {0};
    char key2[32], language[4] = {0};
    char *str = NULL;
    const char *key = NULL;
    uint16_t langcode = 0;
    uint32_t data_type = 0, str_size, str_size_alloc;
    int (*parse)(MOVContext*, AVIOContext*, unsigned, const char*) = NULL;
    int raw = 0;
    int num = 0;

    switch (atom.type) {
    case MKTAG( '@','P','R','M'): key = "premiere_version"; raw = 1; break;
    case MKTAG( '@','P','R','Q'): key = "quicktime_version"; raw = 1; break;
    case MKTAG( 'X','M','P','_'):
        if (c->export_xmp) { key = "xmp"; raw = 1; } break;
    case MKTAG( 'a','A','R','T'): key = "album_artist";    break;
    case MKTAG( 'a','k','I','D'): key = "account_type";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'a','p','I','D'): key = "account_id"; break;
    case MKTAG( 'c','a','t','g'): key = "category"; break;
    case MKTAG( 'c','p','i','l'): key = "compilation";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'c','p','r','t'): key = "copyright"; break;
    case MKTAG( 'd','e','s','c'): key = "description"; break;
    case MKTAG( 'd','i','s','k'): key = "disc";
        parse = mov_metadata_track_or_disc_number; break;
    case MKTAG( 'e','g','i','d'): key = "episode_uid";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'F','I','R','M'): key = "firmware"; raw = 1; break;
    case MKTAG( 'g','n','r','e'): key = "genre";
        parse = mov_metadata_gnre; break;
    case MKTAG( 'h','d','v','d'): key = "hd_video";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'H','M','M','T'):
        return mov_metadata_hmmt(c, pb, atom.size);
    case MKTAG( 'k','e','y','w'): key = "keywords";  break;
    case MKTAG( 'l','d','e','s'): key = "synopsis";  break;
    case MKTAG( 'l','o','c','i'):
        return mov_metadata_loci(c, pb, atom.size);
    case MKTAG( 'm','a','n','u'): key = "make"; break;
    case MKTAG( 'm','o','d','l'): key = "model"; break;
    case MKTAG( 'p','c','s','t'): key = "podcast";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'p','g','a','p'): key = "gapless_playback";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 'p','u','r','d'): key = "purchase_date"; break;
    case MKTAG( 'r','t','n','g'): key = "rating";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 's','o','a','a'): key = "sort_album_artist"; break;
    case MKTAG( 's','o','a','l'): key = "sort_album";   break;
    case MKTAG( 's','o','a','r'): key = "sort_artist";  break;
    case MKTAG( 's','o','c','o'): key = "sort_composer"; break;
    case MKTAG( 's','o','n','m'): key = "sort_name";    break;
    case MKTAG( 's','o','s','n'): key = "sort_show";    break;
    case MKTAG( 's','t','i','k'): key = "media_type";
        parse = mov_metadata_int8_no_padding; break;
    case MKTAG( 't','r','k','n'): key = "track";
        parse = mov_metadata_track_or_disc_number; break;
    case MKTAG( 't','v','e','n'): key = "episode_id"; break;
    case MKTAG( 't','v','e','s'): key = "episode_sort";
        parse = mov_metadata_int8_bypass_padding; break;
    case MKTAG( 't','v','n','n'): key = "network";   break;
    case MKTAG( 't','v','s','h'): key = "show";      break;
    case MKTAG( 't','v','s','n'): key = "season_number";
        parse = mov_metadata_int8_bypass_padding; break;
    case MKTAG(0xa9,'A','R','T'): key = "artist";    break;
    case MKTAG(0xa9,'P','R','D'): key = "producer";  break;
    case MKTAG(0xa9,'a','l','b'): key = "album";     break;
    case MKTAG(0xa9,'a','u','t'): key = "artist";    break;
    case MKTAG(0xa9,'c','h','p'): key = "chapter";   break;
    case MKTAG(0xa9,'c','m','t'): key = "comment";   break;
    case MKTAG(0xa9,'c','o','m'): key = "composer";  break;
    case MKTAG(0xa9,'c','p','y'): key = "copyright"; break;
    case MKTAG(0xa9,'d','a','y'): key = "date";      break;
    case MKTAG(0xa9,'d','i','r'): key = "director";  break;
    case MKTAG(0xa9,'d','i','s'): key = "disclaimer"; break;
    case MKTAG(0xa9,'e','d','1'): key = "edit_date"; break;
    case MKTAG(0xa9,'e','n','c'): key = "encoder";   break;
    case MKTAG(0xa9,'f','m','t'): key = "original_format"; break;
    case MKTAG(0xa9,'g','e','n'): key = "genre";     break;
    case MKTAG(0xa9,'g','r','p'): key = "grouping";  break;
    case MKTAG(0xa9,'h','s','t'): key = "host_computer"; break;
    case MKTAG(0xa9,'i','n','f'): key = "comment";   break;
    case MKTAG(0xa9,'l','y','r'): key = "lyrics";    break;
    case MKTAG(0xa9,'m','a','k'): key = "make";      break;
    case MKTAG(0xa9,'m','o','d'): key = "model";     break;
    case MKTAG(0xa9,'n','a','m'): key = "title";     break;
    case MKTAG(0xa9,'o','p','e'): key = "original_artist"; break;
    case MKTAG(0xa9,'p','r','d'): key = "producer";  break;
    case MKTAG(0xa9,'p','r','f'): key = "performers"; break;
    case MKTAG(0xa9,'r','e','q'): key = "playback_requirements"; break;
    case MKTAG(0xa9,'s','r','c'): key = "original_source"; break;
    case MKTAG(0xa9,'s','t','3'): key = "subtitle";  break;
    case MKTAG(0xa9,'s','w','r'): key = "encoder";   break;
    case MKTAG(0xa9,'t','o','o'): key = "encoder";   break;
    case MKTAG(0xa9,'t','r','k'): key = "track";     break;
    case MKTAG(0xa9,'u','r','l'): key = "URL";       break;
    case MKTAG(0xa9,'w','r','n'): key = "warning";   break;
    case MKTAG(0xa9,'w','r','t'): key = "composer";  break;
    case MKTAG(0xa9,'x','y','z'): key = "location";  break;
    }
retry:
    if (c->itunes_metadata && atom.size > 8) {
        int data_size = avio_rb32(pb);
        int tag = avio_rl32(pb);
        if (tag == MKTAG('d','a','t','a') && data_size <= atom.size && data_size >= 16) {
            data_type = avio_rb32(pb); // type
            avio_rb32(pb); // unknown
            str_size = data_size - 16;
            atom.size -= 16;

            if (!key && c->found_hdlr_mdta && c->meta_keys) {
                uint32_t index = av_bswap32(atom.type); // BE number has been read as LE
                if (index < c->meta_keys_count && index > 0) {
                    key = c->meta_keys[index];
                } else if (atom.type != MKTAG('c', 'o', 'v', 'r')) {
                    av_log(c->fc, AV_LOG_WARNING,
                           "The index of 'data' is out of range: %"PRId32" < 1 or >= %d.\n",
                           index, c->meta_keys_count);
                }
            }
            if (atom.type == MKTAG('c', 'o', 'v', 'r') ||
                (key && !strcmp(key, "com.apple.quicktime.artwork"))) {
                int ret = mov_read_covr(c, pb, data_type, str_size);
                if (ret < 0) {
                    av_log(c->fc, AV_LOG_ERROR, "Error parsing cover art.\n");
                    return ret;
                }
                atom.size -= str_size;
                if (atom.size > 8)
                    goto retry;
                return ret;
            }
        } else return 0;
    } else if (atom.size > 4 && key && !c->itunes_metadata && !raw) {
        str_size = avio_rb16(pb); // string length
        if (str_size > atom.size) {
            raw = 1;
            avio_seek(pb, -2, SEEK_CUR);
            av_log(c->fc, AV_LOG_WARNING, "UDTA parsing failed retrying raw\n");
            goto retry;
        }
        langcode = avio_rb16(pb);
        ff_mov_lang_to_iso639(langcode, language);
        atom.size -= 4;
    } else
        str_size = atom.size;

    if (c->export_all && !key) {
        key = av_fourcc_make_string(tmp_key, atom.type);
    }

    if (!key)
        return 0;
    if (atom.size < 0 || str_size >= INT_MAX/2)
        return AVERROR_INVALIDDATA;

    // Allocates enough space if data_type is a int32 or float32 number, otherwise
    // worst-case requirement for output string in case of utf8 coded input
    num = (data_type >= 21 && data_type <= 23);
    str_size_alloc = (num ? 512 : (raw ? str_size : str_size * 2)) + 1;
    str = av_mallocz(str_size_alloc);
    if (!str)
        return AVERROR(ENOMEM);

    if (parse)
        parse(c, pb, str_size, key);
    else {
        if (!raw && (data_type == 3 || (data_type == 0 && (langcode < 0x400 || langcode == 0x7fff)))) { // MAC Encoded
            mov_read_mac_string(c, pb, str_size, str, str_size_alloc);
        } else if (data_type == 21) { // BE signed integer, variable size
            int val = 0;
            if (str_size == 1)
                val = (int8_t)avio_r8(pb);
            else if (str_size == 2)
                val = (int16_t)avio_rb16(pb);
            else if (str_size == 3)
                val = ((int32_t)(avio_rb24(pb)<<8))>>8;
            else if (str_size == 4)
                val = (int32_t)avio_rb32(pb);
            if (snprintf(str, str_size_alloc, "%d", val) >= str_size_alloc) {
                av_log(c->fc, AV_LOG_ERROR,
                       "Failed to store the number (%d) in string.\n", val);
                av_free(str);
                return AVERROR_INVALIDDATA;
            }
        } else if (data_type == 22) { // BE unsigned integer, variable size
            unsigned int val = 0;
            if (str_size == 1)
                val = avio_r8(pb);
            else if (str_size == 2)
                val = avio_rb16(pb);
            else if (str_size == 3)
                val = avio_rb24(pb);
            else if (str_size == 4)
                val = avio_rb32(pb);
            if (snprintf(str, str_size_alloc, "%u", val) >= str_size_alloc) {
                av_log(c->fc, AV_LOG_ERROR,
                       "Failed to store the number (%u) in string.\n", val);
                av_free(str);
                return AVERROR_INVALIDDATA;
            }
        } else if (data_type == 23 && str_size >= 4) {  // BE float32
            float val = av_int2float(avio_rb32(pb));
            if (snprintf(str, str_size_alloc, "%f", val) >= str_size_alloc) {
                av_log(c->fc, AV_LOG_ERROR,
                       "Failed to store the float32 number (%f) in string.\n", val);
                av_free(str);
                return AVERROR_INVALIDDATA;
            }
        } else if (data_type > 1 && data_type != 4) {
            // data_type can be 0 if not set at all above. data_type 1 means
            // UTF8 and 4 means "UTF8 sort". For any other type (UTF16 or e.g.
            // a picture), don't return it blindly in a string that is supposed
            // to be UTF8 text.
            av_log(c->fc, AV_LOG_WARNING, "Skipping unhandled metadata %s of type %d\n", key, data_type);
            av_free(str);
            return 0;
        } else {
            int ret = ffio_read_size(pb, str, str_size);
            if (ret < 0) {
                av_free(str);
                return ret;
            }
            str[str_size] = 0;
        }
        c->fc->event_flags |= AVFMT_EVENT_FLAG_METADATA_UPDATED;
        av_dict_set(&c->fc->metadata, key, str, 0);
        if (*language && strcmp(language, "und")) {
            snprintf(key2, sizeof(key2), "%s-%s", key, language);
            av_dict_set(&c->fc->metadata, key2, str, 0);
        }
        if (!strcmp(key, "encoder")) {
            int major, minor, micro;
            if (sscanf(str, "HandBrake %d.%d.%d", &major, &minor, &micro) == 3) {
                c->handbrake_version = 1000000*major + 1000*minor + micro;
            }
        }
    }

    av_freep(&str);
    return 0;
}
