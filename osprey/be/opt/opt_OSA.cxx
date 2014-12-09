#include<opt_OSA.h>
#include <fstream>
#include <vector>
#include <string>
#include <glob.h>
using namespace std;


void OSAnode::set_values(INT64 pid, INT64 doru, char (&myopcode)[20], char *varnm, INT64 pline, WN *wn_myself, WN *wn_myparent, char *srcfile, INT64 ismVal, INT64 set_conditional){
id = pid; 
def_id = doru;
strcpy(opcode,myopcode);
var_nm = varnm;
line= pline; 
wn_self = wn_myself;
wn_parent = wn_myparent;
src_file = srcfile;
is_mVal = ismVal;
is_conditional = set_conditional;
openshmem_calls.clear(); 
edges.clear(); 
simpid=-1;
deleted = 0;

}

int OSAgraph::CFGIsOpenSHMEM(char *input, int begin, int end) {
  int debug=0;
  char shmem_name[190][50] ={
    "first_name",
    "start_pes",   // 1
    "shmem_init",
    "shmem_finalize",
    "shmem_my_pe",  // 4
    "my_pe",
    "_my_pe",
    "shmem_num_pes",
    "shmem_n_pes",
    "num_pes",
    "_num_pes",
    "shmem_nodename",
    "shmem_version"
  };
}

string OSAgraph::getcallcolor(string name) {
        static string call;
        char *node_name = (char *)name.c_str();
           if(CFGIsOpenSHMEM(node_name,98,107) || CFGIsOpenSHMEM(node_name,82,85) || CFGIsOpenSHMEM(node_name,186,188))
             call="red";
            else if (CFGIsOpenSHMEM(node_name,1,12)) // init, runtime queries
              call="green";
           else if (CFGIsOpenSHMEM(node_name,88,97)) // symetic memory management
             call="yellow";
           else if (CFGIsOpenSHMEM(node_name,108,128)) // atomics
             call="orange";
           else if (CFGIsOpenSHMEM(node_name,135,178)) // reductions
             call="purple";
           else if (CFGIsOpenSHMEM(node_name,179,181)) // broadcast
             call="red";
            else if (CFGIsOpenSHMEM(node_name,135,178)) // reductions
              call="purple";
           else if (CFGIsOpenSHMEM(node_name,13,81)) // IO
             call="blue";
           else // all others
             call="black";
           return call;

}
void OSAgraph::relabel(void) {
/* TODO: strcpy every opcode
 *
     for(INT64 i=0;i<nodes.size();i++) {
       if(strcmp(nodes[i].opcode,"GOTO") == 0)
         {
           strcpy(nodes[i].opcode,"STMTS");
         }
       if(strcmp(nodes[i].opcode,"LOGIF") == 0)
         {
           nodes[i].opcode = "BRANCH";
         }
       if(strcmp(nodes[i].opcode,"WHILEEND") == 0)
         {
           nodes[i].opcode = "LOOP";
         }
       if(strcmp(nodes[i].opcode,"ENTRY") == 0)
         {
           nodes[i].opcode = "ENTRY";
         }
       if(strcmp(nodes[i].opcode,"EXIT") == 0)
         {
           nodes[i].opcode = "EXIT";

         }
     }
*/
}



void OSAgraph::printnode(int id) {
  string sourcefile = Src_File_Name;
  char charid[10000];
  string nodename=nodes[id].opcode;
  //sprintf(charid,"%d",nodes[id].ID());
  nodename+=charid;
  string nodelabel=nodes[id].opcode;

 if(nodes[id].line) {
    char charid2[10000];
    sprintf(charid2,"%d",nodes[id].line);
    nodelabel  +="(";
    nodelabel +=charid2;
    nodelabel +=")";
  }

  string labelcolor="black";
  ofstream fout;
  fout << "\""<<nodename <<"\" [ style = \"filled\" penwidth = 1 fillcolor = \"white\" fontname = \"Courier New\" shape = \"Mrecord\" label =<<table border=\"0\" cellborder=\"\" cellpadding=\"2\" bgcolor=\"white\"> <tr><td bgcolor=\""<<labelcolor<<"\" align=\"center\" colspan=\"1\"><font color=\"white\">"<<nodelabel<<"</font></td></tr>"<<endl;

    for(int i=0; i<nodes[id].openshmem_calls.size(); i++) {
      string callcolor = getcallcolor(nodes[id].openshmem_calls[i].callname);
      int j=i+1;
      fout << "<tr><td align=\"left\" port=\"r"<<j<<"\">&#40;"<<i<<"&#41; "<<nodes[id].openshmem_calls[i].callname<<"</td><td bgcolor=\""<<callcolor<<"\" align=\"right\">"<<nodes[id].openshmem_calls[i].line <<"</td></tr>" <<endl;
   }
    fout <<"</table>>" << " URL=\"file:" << sourcefile<<".html#line"<< nodes[id].line <<"\""<<" ];" << endl;


}
void OSAgraph::print(void) {

   int i;
   string pu_file_name = Cur_PU_Name;

   if(Control_Flow_Type_Num!=-1) {
    char charid3[10000];
    sprintf(charid3,"%d",Control_Flow_Type_Num);
    pu_file_name  +="-pe";
    pu_file_name +=charid3;
   }

   ofstream fout;
   pu_file_name+=".dot";
   fout.open(pu_file_name.c_str(),ios::out);
   fout << "digraph flowgraph {\n";
   fout << "node [color=grey, style=filled];\n";
   fout << "node [fontname=\"Verdana\", size=\"30,30\"];\n";
    if(Control_Flow_Type_Num==-1)
   fout << "graph [ fontname = \"Arial\",fontsize = 20,style = \"bold\",label = \"flowgraph: " << Cur_PU_Name <<"\",ssize = \"30,60\"];\n";
   else
     fout << "graph [ fontname = \"Arial\",fontsize = 20,style = \"bold\",label = \"flowgraph: " << Cur_PU_Name <<" pe="<<Control_Flow_Type_Num <<"\",ssize = \"30,60\"];\n";
   for(int i=0; i< nodes.size();i++) {
     //if (nodes[i].deleted) continue;
     printnode(i);
     for(int j=0; j<nodes[i].edges.size(); j++) {
       for(int k=0; k< nodes.size(); k++){ //traverse all nodes to find the 'other end'
            if(nodes[i].edges[j].use_id == nodes[k].id ){
            fout << nodes[i].opcode<<nodes[i].ID()<< " -> " << nodes[k].id << endl;      
      }
   }
     fout << endl;
     fout << "{ rank = sink; Legend [shape=none, margin=0, label=<" <<endl;
     fout << "<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"1\">" << endl;
     fout << "<TR><TD align=\"left\">Legend                </TD><TD>  </TD></TR>" << endl;
     fout << "<TR><TD align=\"left\">I/O                   </TD> <TD BGCOLOR=\"blue\"></TD></TR>" << endl;
     fout << "<TR><TD align=\"left\">Reductions            </TD> <TD BGCOLOR=\"purple\"></TD></TR>" << endl;
     fout << "<TR><TD align=\"left\">Broadcast             </TD> <TD BGCOLOR=\"red\"></TD></TR>" << endl;
     fout << "<TR><TD align=\"left\">Atomics               </TD> <TD BGCOLOR=\"orange\"></TD></TR>" << endl;
     fout << "<TR><TD align=\"left\">Memory Mgt            </TD> <TD BGCOLOR=\"yellow\"></TD></TR>" << endl;
     fout << "<TR><TD align=\"left\">State Queries         </TD> <TD BGCOLOR=\"green\"></TD></TR>" << endl;
     fout << "<TR><TD align=\"left\">Syncrhonizations      </TD> <TD BGCOLOR=\"red\"></TD></TR>" << endl;
     fout << "</TABLE> >]; }" << endl;
     fout << "}\n";
   fout.close();
}
}
}

