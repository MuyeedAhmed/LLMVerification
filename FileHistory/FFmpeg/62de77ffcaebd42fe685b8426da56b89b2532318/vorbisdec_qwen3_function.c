Do not provide any explanations. Do not repeat the original code. Do not use markdown formatting. Do not use backticks. Do not use code blocks. Do not use "```".

vorbis_codebook codebook = vc->codebooks[vf->book_list[book_idx]];
        /* Invalid codebook! */
        if (!codebook.codevectors)
            return AVERROR_INVALIDDATA;

        while (lsp_len<vf->order) {
            int vec_off;

            av_dlog(NULL, "floor0 dec: book dimension: %d\n", codebook.dimensions);
            av_dlog(NULL, "floor0 dec: maximum depth: %d\n", codebook.maxdepth);
            /* read temp vector */
            vec_off = get_vlc2(&vc->gb, codebook.vlc.table,
                               codebook.nb_bits, codebook.maxdepth)
                      * codebook.dimensions;
            av_dlog(NULL, "floor0 dec: vector offset: %d\n", vec_off);
            /* copy each vector component and add last to it */
            for (idx = 0; idx < codebook.dimensions; ++idx)
                lsp[lsp_len+idx] = codebook.codevectors[vec_off+idx] + last;
            last = lsp[lsp_len+idx-1]; /* set last to last vector component */

            lsp_len += codebook.dimensions;
        }
