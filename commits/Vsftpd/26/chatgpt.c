int
str_getline(const struct mystr* p_str, struct mystr* p_line_str,
            unsigned int* p_pos)
{
  unsigned int start_pos = *p_pos;
  unsigned int curr_pos = start_pos;
  unsigned int buf_len = str_getlen(p_str);
  const char* p_buf = str_getbuf(p_str);
  unsigned int out_len;

  if (start_pos > buf_len)
  {
    bug("p_pos out of range in str_getline");
  }
  str_empty(p_line_str);
  if (start_pos == buf_len)
  {
    return 0;
  }
  while (curr_pos < buf_len && p_buf[curr_pos] != '\n')
  {
    if (curr_pos + 1 < curr_pos)  // Check for integer overflow
    {
      bug("Integer overflow detected in str_getline");
    }
    curr_pos++;
  }
  out_len = curr_pos - start_pos;
  /* If we ended on a \n - skip it */
  if (curr_pos < buf_len && p_buf[curr_pos] == '\n')
  {
    curr_pos++;
  }
  private_str_alloc_memchunk(p_line_str, p_buf + start_pos, out_len);
  *p_pos = curr_pos;
  return 1;
}