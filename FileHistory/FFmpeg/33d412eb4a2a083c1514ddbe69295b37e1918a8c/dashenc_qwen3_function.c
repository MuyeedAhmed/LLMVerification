Do not provide any explanation or any other text. Do not repeat the prompt. Do not use markdown formatting. Do not use code blocks. Do not use "```".

int dash_write(void *opaque, uint8_t *buf, int buf_size)
{
    OutputStream *os = opaque;
    if (os->out)
        ffurl_write(os->out, buf, buf_size);
    return buf_size;
}
