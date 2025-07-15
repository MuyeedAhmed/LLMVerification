/* The tag to show in the generated parsers.  Use "end of file" rather
   than "$end".  But keep "$end" in the reports, it's shorter and more
   consistent.  Support i18n if the user already uses it.  */
   static const char *
   symbol_tag (const symbol *sym)
   {
     const bool eof_is_user_defined
       = !endtoken->alias || STRNEQ (endtoken->alias->tag, "$end");
   
     if (!eof_is_user_defined && sym->content == endtoken->content)
       return "\"end of file\"";
     else if (sym->content == undeftoken->content)
       return "\"invalid token\"";
     else
       return sym->tag;
   }