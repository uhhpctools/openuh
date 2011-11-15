/* a class of Control Flow Graph Node */
#ifndef FGnode_INCLUDED
#define FGnode_INCLUDED

#include <vector>
#include <fstream>
#include <iostream>
using namespace std;
#include "opt_bb.h"

#ifndef fb_whirl_INCLUDED   
#include "fb_whirl.h"  // For feedback information, Added by Liao,3/2/04
#endif

class FGRAPH_MEM {
protected:
  MEM_POOL _m;
  
  FGRAPH_MEM() {
    MEM_POOL_Initialize( &_m, "FGRAPH_MEM", true );
    MEM_POOL_Push( &_m );
  }
  ~FGRAPH_MEM() {   
    MEM_POOL_Pop( &_m );
    MEM_POOL_Delete(&_m );
  }
};

//----------------------
// Added by C.H. Liao, 3/2/04
// To support feedback information about control flow graph edges
// Using OPT_FB_EDGE as template here

class FGEDGE {

 public:
  
  IDTYPE       identity; 
  IDTYPE       source;
  IDTYPE       destination;

  FB_EDGE_TYPE edge_type;
  FB_FREQ      freq;

  FGEDGE() {};
  FGEDGE(IDTYPE src, IDTYPE dst): 
    identity(0),
    source(src),
    destination(dst){
    edge_type=FB_EDGE_UNINIT;
    freq._type=FB_FREQ_TYPE_UNINIT;
    freq._value=-1.0;
  }
  FGEDGE(IDTYPE id, IDTYPE src, IDTYPE dst,
	       FB_EDGE_TYPE typ  = FB_EDGE_UNINIT,
	       FB_FREQ      freq = FB_FREQ_UNINIT ) :
    identity(id),
    source(src),
    destination(dst),
    edge_type(typ),
    freq(freq) {}
 /*
  void Print( IDTYPE id, FILE *fp = stderr ) const {
    char buffer[FGEDGE_TYPE_NAME_LENGTH];
    FB_EDGE_TYPE_sprintf( buffer, edge_type );

    fprintf( fp, "Edge[%3d]:  (%3d --> %3d) : freq = ",
	     id, source, destination );
    freq.Print( fp );
    fprintf( fp, " : %s\n", buffer );
    } 
*/
  // write the content of this object into a file
  void Print(ofstream &outfile){

 outfile.write((char *) &identity, sizeof(IDTYPE)); 	
 outfile.write((char *) &source, sizeof(IDTYPE));
 outfile.write((char *) &destination, sizeof(IDTYPE));

 outfile.write((char *) &edge_type, sizeof(FB_EDGE_TYPE));
 outfile.write((char *) &freq._type, sizeof(FB_FREQ_TYPE));
 outfile.write((char *) &freq._value, sizeof(float)); 

  }// END of Print

  FGEDGE (ifstream &fin) // Construct an object from file stream
    {
     
  FB_FREQ_TYPE f_type;
      float f_value;

  fin.read((char *)&identity, sizeof(IDTYPE));  // get ID
  fin.read((char *)&source, sizeof(IDTYPE));
  fin.read((char *)&destination, sizeof(IDTYPE));

  fin.read((char *) &edge_type, sizeof(FB_EDGE_TYPE));
  fin.read((char *) &f_type, sizeof(FB_FREQ_TYPE));
  fin.read((char *) &f_value, sizeof(float));

  freq=FB_FREQ(f_type, f_value);


    }// end of constructor from file stream

  void Print(){
    char buffer[FB_EDGE_TYPE_NAME_LENGTH];
    FB_EDGE_TYPE_sprintf( buffer, edge_type );

    // cout<<"An edge of CFG:"<<endl;
    cout<<source<<"--"<<identity<<"-->"<<destination<<endl;
    cout<<"Edge type"<<buffer<<endl;
    freq.Sprintf(buffer);
    cout <<"Freqency:"<<buffer<<endl;
  }
};

class FGNODE
{
    private:

	 char Label[1000];	
	 unsigned int ID;	   // basic block ID
	 vector<unsigned int>preds;  // parents
	 vector<unsigned int>succs;  // childen

	 vector <FGEDGE *> in_edge_vec;  //Added by Liao
	 vector <FGEDGE *> out_edge_vec; // For edge feedback information
	 
	 void clear_vec_des(){
	/*  
          for (int i=0;i<in_edge_vec.size();i++) 
	     if (in_edge_vec[i]) { delete in_edge_vec[i]; in_edge_vec[i]=NULL;}
        
	   for (int i=0;i<out_edge_vec.size();i++)
	     if (out_edge_vec[i]) {delete out_edge_vec[i]; out_edge_vec[i]=NULL;}
         */
	   preds.clear();
	   succs.clear();
	   in_edge_vec.clear();
	   out_edge_vec.clear();
	   
	 }
         void clear_vec(){};

    public:

	vector<unsigned int> line_nums;
	FGNODE(){
	  clear_vec();	  	  
	};
	FGNODE(unsigned int id, char* label){
	  clear_vec();
	  ID = id;
	  strcpy(Label,label);
	}
	~FGNODE() {
	  clear_vec_des();
	};

	void Insert_line_num(int line_num){
		line_nums.push_back(line_num);
	}
	void Insert_succ(unsigned int id){
	succs.push_back(id);
		}
	// Liao
	void Insert_succ(FGEDGE *o_vec){
	  out_edge_vec.push_back(o_vec);
	}

	void Insert_pred(unsigned int id){
		preds.push_back(id);
		}
	//Liao
	void Insert_pred(FGEDGE *i_vec){
	  in_edge_vec.push_back(i_vec);
	}
	unsigned int GetId(){ return ID; }

	void Print_pred();
	void Print_succ();
	void Print_line_num();
	void Print();
	void Print_pred(FILE *file);
	void Print_succ(FILE *file);
	void Print_line_num(FILE *file);
	void Print(FILE *file);
	//Modified by Liao, 3/3/04
	//For feedback supporting
// The interface between open64 and Dragon
//Node level dump
//
	void Print(ofstream &outfile){
		int len;
		outfile.write((char *) &ID, sizeof(int));	// ID
		len = strlen(Label);
		outfile.write((char *) &len, sizeof(int));	// Label
		outfile.write(Label, len);
		len = line_nums.size();
		outfile.write((char *) &len, sizeof(int));  // number of line no.
		for(int i=0; i<len; i++)		// line no.
			outfile.write((char *) &line_nums[i], sizeof(int));
		
//1. WRITE info. for predecessors of current node
                //len = preds.size();
		len=in_edge_vec.size();		
                outfile.write((char *) &len, sizeof(int));
		for(int i=0; i<len; i++)
		  {	
	  	outfile.write((char *) &preds[i], sizeof(int));
     // output all necessary info. for the edge instead of only caller id
		  in_edge_vec[i]->Print(outfile);
		  }
//2. WRITE info. for successors of the node                 
               //len = succs.size();
		len=out_edge_vec.size();		
               outfile.write((char *) &len, sizeof(int));  // number 
		for(int i=0; i<len; i++)		// successors
		  {
		    outfile.write((char *) &succs[i], sizeof(int));	
		    out_edge_vec[i]->Print(outfile);
		  }
	}
//	void Print(ofstream &outfile);
//  Modified by Liao, 3/6/2004,
// Take into account the size of edges for each node

	unsigned int GetSize(){
	unsigned int len=0, len_int,edge_size;
	len_int = sizeof(int);
	edge_size=3*sizeof(IDTYPE)+sizeof(float)+
	  sizeof(FB_EDGE_TYPE)+sizeof(FB_FREQ_TYPE);
	len += len_int;		// ID
	len += len_int;		// Label
	len += strlen(Label);   
	len += len_int;		// line_nums
	len += line_nums.size()*len_int;	
	len += len_int;		// predecessors
	len += in_edge_vec.size()*(len_int+ edge_size);	
	len += len_int;		// successors
	len += out_edge_vec.size()*(len_int + edge_size);
	return len;
}
	FGNODE& operator= (const BB_NODE *bb_node);
};


class FGRAPH : public FGRAPH_MEM
{

	private:
		char  pu_name[1000];
		BOOL  Finished;


	public:
		vector<FGNODE, mempool_allocator<FGNODE> > FG_Nodes;
//		vector<FGNODE_TYPE> FG_Nodes;

		FGRAPH(){Finished = FALSE;}
		FGRAPH(char* name){
//			pu_name = CXX_NEW(char[strlen(pu_name)+1],Malloc_Mem_Pool);
			strcpy(pu_name, name);
			Finished = FALSE;
		}

		~FGRAPH(){
//			CXX_DELETE(pu_name,Malloc_Mem_Pool);
		}
		void Insert_FNode(FGNODE node){
			FG_Nodes.push_back(node);
		}
		BOOL If_Finished(){ return Finished; }
		void Set_Finished(BOOL finish){ Finished = finish; }
		void Set_pu_name(char* name){
//			pu_name = CXX_NEW(char[strlen(pu_name)+1],Malloc_Mem_Pool);
			strcpy(pu_name, name);
		}
		//Modified by Liao. for Debugging 	
	void Print(){
			cout<< "\n ---- " << pu_name <<" ------------\n";
			for(int i=0; i<FG_Nodes.size(); i++){
			  	FG_Nodes[i].Print();
				//cout<<"------->";
			  // FG_Nodes[i].Print_succ();
			  // FG_Nodes[i].Print_pred();
			}
		}
		void Print(FILE *file);

//Used for write out Control flow graph information 
//  The interface between open64 and Dragon
//
//   Modified by Liao
//----------------------------------		
 void Print(ofstream &outfile)
	{
			int len;
			len = GetSize();	
// get size of the control flow graph
//			cout << "size === " << len <<"\n";
			outfile.write((char *) &len, sizeof(int));	
			len = strlen(pu_name);			// pu_name
			outfile.write((char *) &len, sizeof(int));
			outfile.write(pu_name,len);
			len = FG_Nodes.size();
			outfile.write((char *) &len, sizeof(int));     
 // Nodes number
//			cout<<"\nsize of nodes "<<len<<"\n";
			for(int i=0; i<len; i++)		
    // store each nodes information
				FG_Nodes[i].Print(outfile);
		}
//		void Print(ofstream &outfile);

       unsigned int GetSize()   
// calculate the length of a flow graph saved in a file
		{
			unsigned int len=0, len_int;
			len_int = sizeof(int);
			len += len_int;
			len += strlen(pu_name);    // pu_name 
			len += len_int;		   // Nodes number
			for(int i=0; i<FG_Nodes.size(); i++){
				len += FG_Nodes[i].GetSize();
			}	
			return len;
		}				
// unsigned int GetSize();
		void Load_CFG(char* filename, char* pu);
};

// extern vector<FGRAPH, mempool_allocator<FGRAPH> > CFGRAPH;
extern FGRAPH Curr_CFG;


#endif

