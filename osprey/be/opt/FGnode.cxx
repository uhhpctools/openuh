/* Control Flow graph class  07/31/2002  Lei Huang */

#include <vector>
using namespace std;
#include "FGnode.h"
#include "opt_bb.h"

//FGRAPH Curr_CFG;

FGNODE&
FGNODE::operator= (const BB_NODE *bb_node){

	ID = bb_node->Id();
	strcpy(Label, bb_node->Kind_name());
	return *this;
}


void 
FGNODE::Print_pred(){
  cout <<"old information"<<endl;  
	for(int i=0; i<preds.size(); i++)
  		cout<<preds[i]<<" ";
  cout<<"Predecessors:"<<endl;
  for (int i=0;i<in_edge_vec.size();i++)
    in_edge_vec[i]->Print();
}
	
void
FGNODE::Print_succ(){
  cout <<"old information"<<endl;  
  for(int i=0; i<succs.size(); i++)
 		cout<<succs[i]<<" ";
  cout <<"Successors:"<<endl;
 for (int i=0;i<out_edge_vec.size();i++)
   out_edge_vec[i]->Print();
}
	
void
FGNODE::Print_line_num(){
		for(int i=0; i<line_nums.size(); i++)
			cout<<line_nums[i]<<" ";
}

void 
FGNODE::Print_pred(FILE *file){
		fprintf(file,"Preds: ");
		//	for(int i=0; i<preds.size(); i++)
for(int i=0; i<in_edge_vec.size(); i++)
		  //fprintf(file, "%i ", preds[i]);
		  {
    fprintf(file,"Caller:%i Edge: %i, Frequency: ", 
                  in_edge_vec[i]->source, in_edge_vec[i]->identity);
		in_edge_vec[i]->freq.Print(file);
		  }
		  
		fprintf(file,"\n");
}
	
void
FGNODE::Print_succ(FILE *file){
		fprintf(file,"Succs: ");
		//	for(int i=0; i<succs.size(); i++)
for(int i=0; i<out_edge_vec.size(); i++)
		  //	fprintf(file,"%i ", succs[i]);
		  {
 fprintf(file,"Callee:%i Edge: %i, Frequency: ", 
                  out_edge_vec[i]->destination, out_edge_vec[i]->identity);
		out_edge_vec[i]->freq.Print(file);
		  }
		fprintf(file,"\n");
}
	
void
FGNODE::Print_line_num(FILE *file){
		fprintf(file,"Line_no: ");
		for(int i=0; i<line_nums.size(); i++)
			fprintf(file, "%i ", line_nums[i]);
		fprintf(file,"\n");
}

	
void
FGNODE::Print(){

  cout<<"\nNode ID=="<<ID<<", Label=="<<Label<<"\n -->";
  cout<<"\n line no.:";
  Print_line_num();
  cout<<"\n";

  // cout<<"||Predecessors:"<<end;
   Print_pred();
	  
  // cout<<"||Successors:"<<endl;
  Print_succ();
		
}
	
void 
FGNODE::Print(FILE *file){
		fprintf(file,"\n");
		Print_pred(file);
		fprintf(file, "ID: %i Label: %s\n", ID, Label);
		Print_line_num(file);
		Print_succ(file);
		cout<<"\n";
}



void 
FGRAPH::Print(FILE *file){
	fprintf(file, "\n PU_NAME: %s\n", pu_name);
	for(int i=0; i<FG_Nodes.size(); i++){
		FG_Nodes[i].Print(file);
	}
}




// Not actually used in Open64, so no update 
// see Load_CFG in dragon for up-to-date code
//Comments: By Liao
		
void 
FGRAPH::Load_CFG(char* filename, char* pu)
{
	ifstream fin(filename,ios::in);
//	ifstream fin(filename,ios::in | ios::binary);
 //Added by Liao for reading feedback info. 3/2/04
	FGEDGE* tempEd;
	unsigned int src, id;
        float freq;
	
	if(fin.fail()){ 
cout << "Error opening control flow graph file:" << filename << "\n";
		return;
	}

	fin.seekg(ios::beg);
	while(!fin.eof()){	
		int size;
		fin.read((char *)&size,sizeof(int));
		if(fin.eof())
			break;
		int len;
		fin.read((char *)&len, sizeof(int));
		char data[1000];
		fin.read(data, len);
		data[len]='\0';

		if(strcmp(data,pu)==0){  // find the pu's cfg
			cout<<"found pu!\n";
			Set_pu_name(pu);
			int id,num;
	fin.read((char *)&num, sizeof(int));  // get CFG nodes number
			for(int i=0; i<num; i++){
				fin.read((char *)&id, sizeof(int));  // get ID
				fin.read((char *)&len, sizeof(int));	
				fin.read(data,len);		// get Label;
				data[len]='\0';
				FGNODE fnode(id, data);		
				int size,line,pred,succ;
				fin.read((char *)&size, sizeof(int));  // get line_num size
				for(int j=0; j<size; j++){
		fin.read((char *)&line, sizeof(int)); // get line No.
					fnode.Insert_line_num(line);
				}
 //1. read in info. for predecessors
		fin.read((char *)&size, sizeof(int));  // get predecessors size
				for(int j=0; j<size; j++){

	fin.read((char *)&pred, sizeof(int)); // get predecessors.
	tempEd=new  FGEDGE(fin);	
				  //	fnode.Insert_pred(pred);
				  fnode.Insert_pred(tempEd);
				}
				
				
				fin.read((char *)&size, sizeof(int));  // get successors size
				for(int j=0; j<size; j++){
 fin.read((char *)&succ, sizeof(int)); // get successors
				tempEd=new FGEDGE(fin);	

				  //	fnode.Insert_succ(succ);
				  fnode.Insert_succ(tempEd);
				}
				Insert_FNode(fnode);
			}
			break;
		}			

		fin.seekg(size-len-sizeof(int),ios::cur);
	}
	fin.close();
	
}

