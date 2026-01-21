Do not provide any extra information.

```c
static void ffmpeg_cleanup(int ret)
{
    if ((print_graphs || print_graphs_file) && nb_output_files > 0)
        print_filtergraphs(filtergraphs, nb_filtergraphs, input_files, nb_input_files, output_files, nb_output_files);

    if (do_benchmark) {
        int64_t maxrss = getmaxrss() / 1024;
        av_log(NULL, AV_LOG_INFO, "bench: maxrss=%"PRId64"KiB\n", maxrss);
    }

    for (int i = 0; i < nb_filtergraphs; i++)
        fg_free(&filtergraphs[i]);
    av_freep(&filtergraphs);

    for (int i = 0; i < nb_output_files; i++)
        of_free(&output_files[i]);

    for (int i = 0; i < nb_input_files; i++)
        ifile_close(&input_files[i]);

    for (int i = 0; i < nb_decoders; i++)
        dec_free(&decoders[i]);
    av_freep(&decoders);

    if (vstats_file) {
        if (fclose(vstats_file))
            av_log(NULL, AV_LOG_ERROR,
                   "Error closing vstats file, loss of information possible: %s\n",
                   av_err2str(AVERROR(errno)));
    }
    av_freep(&vstats_filename);
    of_enc_stats_close();

    hw_device_free_all();

    av_freep(&filter_nbthreads);

    av_freep(&input_files);
    av_freep(&output_files);

    uninit_opts();

    avformat_network_deinit();

    if (received_sigterm) {
        av_log(NULL, AV_LOG_INFO, "Exiting normally, received signal %d.\n",
               (int) received_sigterm);
    } else if (ret && atomic_load(&transcode_init_done)) {
        av_log(NULL, AV_LOG_INFO, "Conversion failed!\n");
    }
    term_exit();
    ffmpeg_exited = 1;
}
```
