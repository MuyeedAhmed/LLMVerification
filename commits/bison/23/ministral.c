static void
start_symbols_output (FILE *out)
{
    if (nstart > 1)
    {
        fprintf (out, "m4_define([b4_start_symbols],\n[");
        for (int i = 0; i < nstart; ++i)
        {
            fprintf (out, "[%d, %d]", start_symbols[i], start_symbols[i]);
            if (i < nstart - 1)
                fprintf (out, ", ");
        }
        fprintf (out, "])\n\n");
    }
}
