No explanation needed.
```c
void ff_flush_packet_queue(AVFormatContext *s)
{
    FormatContextInternal *const fci = ff_fc_internal(s);
    FFFormatContext *const si = &fci->fc;

    avpriv_packet_list_free(&fci->parse_queue);
    avpriv_packet_list_free(&si->packet_buffer);
    avpriv_packet_list_free(&fci->raw_packet_buffer);

    fci->raw_packet_buffer_size = 0;
}
```
