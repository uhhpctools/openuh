
//dragon merge
void ACCESS_ARRAY::Dragon_Print(ofstream &outfile,BOOL is_bound) const
{
  int numvec=0;

   if (Too_Messy) {
     numvec = 1;
     outfile.write((char *) &numvec, sizeof(int));
     char messy[]="[EXPR]";
     int name_len = strlen(messy)+1;
     outfile.write((char *) &name_len, sizeof(int));
     outfile.write((char *) messy, name_len);
    return;
  }

   numvec = _num_vec;
   outfile.write((char *) &numvec, sizeof(int));

  for (INT32 i=0; i<_num_vec; i++) {
    Dim(i)->Dragon_Print(outfile,is_bound);
  }


}


//dragon merge
void ACCESS_VECTOR::Dragon_Print(ofstream &outfile, BOOL is_bound, BOOL print_brackets) const
{
  char bf[MAX_TLOG_CHARS];
  Print(bf, 0, is_bound, print_brackets);
  int name_len = strlen(bf)+1;
  outfile.write((char *) &name_len, sizeof(int));
  outfile.write((char *) bf, name_len);
}

 //dragon merge
/****************************************
 ** Write dependence graph into a file **
 ****************************************/
 void  write_dependence_graph(WN *func_nd)
 {
    static bool firsttime = true;
    const char *filename;
    const char *dirname;
    char *dumpfilename;
    int i;


    SRCPOS srcpos = WN_Get_Linenum(func_nd);
    USRCPOS linepos;
    USRCPOS_srcpos(linepos) = srcpos;
    IR_Srcpos_Filename(srcpos,&filename,&dirname);

    dumpfilename = new char[strlen(filename)+10];
    strcpy(dumpfilename, filename);

   for (i=0; i <strlen(filename); i++)
         {
          if (dumpfilename[i]=='.')
            {
               dumpfilename[i+1]='\0';
               strcat(dumpfilename,"dep");
               break;
             }
         }
       if (i==strlen(dumpfilename))
        strcat(dumpfilename,".dep");
       if (firsttime)
        {
               ofstream out(dumpfilename, ios::out | ios::binary);
               out.close();
               firsttime=false;
         }
       Array_Dependence_Graph->Dragon_Print(dumpfilename,func_nd);

       delete [] dumpfilename;
 }

