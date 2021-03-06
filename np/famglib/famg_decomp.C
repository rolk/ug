/****************************************************************************/
/*																			*/
/* File:      famg_decomp.C													*/
/*																			*/
/* Purpose:   famg decomp classes functions									*/
/*																			*/
/* Author:    Christian Wagner												*/
/*			  Institut fuer Computeranwendungen  III						*/
/*			  Universitaet Stuttgart										*/
/*			  Pfaffenwaldring 27											*/
/*			  70569 Stuttgart												*/
/*			  internet: chris@ica3.uni-stuttgart.de							*/
/*																			*/
/*																			*/
/* History:   November 97 begin, Stuttgart									*/
/*			  August 98 integration into ug (Christian Wrobel)				*/
/*																			*/
/* Remarks:																	*/
/*																			*/
/****************************************************************************/

#include "config.h"
#include <iostream.h>
#include "famg_decomp.h"
#include "famg_heap.h"
#include "famg_misc.h"
#include "famg_system.h"

/* RCS_ID
$Header$
*/

// Class FAMGDecompEntry

void FAMGDecompEntry::Init(int i)
{
    id.f0 = i;
    next = NULL;
    data = 0.0;

    return;
}        

    // Class FAMGDecompRow


FAMGDecompEntry *FAMGDecompRow::NewEntry(FAMGDecompRow* rowj)
{
    FAMGDecompEntry *matij, *matji, *ptr;
    int i,j;

    // Allocate matij and matji at once. That's not nice but it saves
    // an pointer.

    ptr = (FAMGDecompEntry *) FAMGGetMem(sizeof(FAMGDecompEntry[2]),FAMG_FROM_TOP);
    if(ptr == NULL)  return(NULL);

    i = id;
    j = rowj->GetId();
    ptr->SetRev(0);
    (ptr+1)->SetRev(1);
    matij = ptr;
    matji = ptr+1;

    matij->SetData(0.0);
    matji->SetData(0.0);
    matij->SetId(j);
    matji->SetId(i);

    matij->SetNext(right);
    matji->SetNext(rowj->GetRight());
    right = matij;
    rowj->SetRight(matji);

    return matij;
}
    
FAMGDecompEntry *FAMGDecompRow::GetEntry(int j) const
{
    FAMGDecompEntry *matij;

    for(matij = right; matij != NULL; matij = (*matij).GetNext())
    {
        if(matij->GetId() == j) return matij;
    }

    return NULL;
}

int FAMGDecompRow::SaveEntry(FAMGDecompRow* rowj, double val)
{
    FAMGDecompEntry *matij;
    int j;

    // test 
    // if(Abs(val) < 1e-2) return 0;

    j = rowj->GetId();
    if(id == j) // diagonal entry
    {
        data += val;
        return 0;
    }
    matij = GetEntry(j);
    if(matij == NULL)
    {
        matij = NewEntry(rowj);
        if(matij == NULL) return 1;
    }

    matij->AddData(val);
    return 0;
}


int FAMGDecompRow::SaveEntry(FAMGDecompRow* rowj, double val,FAMGDecompEntry** p)
{
    FAMGDecompEntry *matij;
    int j;

    *p = NULL;    
    // test 
    // if(Abs(val) < 1e-2) return 0;

    j = rowj->GetId();
    if(id == j) // diagonal entry
    {
        data += val;
        return 0;
    }
    matij = GetEntry(j);
    if(matij == NULL)
    {
        matij = NewEntry(rowj);
        if(matij == NULL) return 1;
    }

    matij->AddData(val);
    *p = matij;
    return 0;
}

void FAMGDecompRow::MoveLeft(FAMGDecompEntry* mat)
{
    FAMGDecompEntry *link, *prev;

    if (right == mat) right=mat->GetNext();
    else
    {
        prev = right;
        for(link = right->GetNext(); link != NULL; link = link->GetNext())
        {
            if(link == mat) break;
            prev = link;
        }
        prev->SetNext(mat->GetNext());
    }
    mat->SetNext(left);
    left = mat;
}

void FAMGDecompRow::Init(int i)
{
    id = i;
    right = NULL;
    left = NULL;
    data = 0.0;

    return;
}        


    // Class FAMGDecomp

        
int FAMGDecomp::Init(FAMGMatrix *matrix) 
{
    FAMGDecompRow *rowi;
    FAMGMatrixPtr matij;
    int i;

    n = matrix->GetN();

    row = (FAMGDecompRow*) FAMGGetMem(n*sizeof(FAMGDecompRow), FAMG_FROM_TOP);
    if (row == NULL) return(1);
    for(i = 0; i < n; i++)
    {
        rowi = row+i;
        rowi->Init(i);
    }
    
    /* copy matrix */
    for(i = 0; i < n; i++)
    {
        rowi = row+i;
        matij = matrix->GetStart(i);
        rowi->SetData(matij.GetData());
        while(matij.GetNext())
        {
            if(rowi->SaveEntry(row+(matij.GetIndex()),matij.GetData())) return 1;
        }
    }

    return(0);
} 


int FAMGDecomp::Construct(int cgilut)	
{
    FAMGDecompRow *rowi, *rowj, *rowk;
    FAMGDecompEntry *matij, *matik, *matji, *matjk;
	double factor,val,diag, mjj, mkk, ilut;
    int i,j,k;

    if(cgilut) ilut = FAMGGetParameter()->Getcgilut();
    else ilut = FAMGGetParameter()->Getilut();
    
    for(i = 0; i < n; i++)
    {
        rowi = row+i;
        diag = rowi->GetData();
        if(Abs(diag) < 1e-15)
        {
            ostrstream ostr; ostr << __FILE__ << ", line " << __LINE__ << ": division by zero" << endl;
            FAMGWarning(ostr);
        }

        // Elimination of the whole Column 
        for(matij = rowi->GetRight();  matij != NULL; matij = matij->GetNext())
        {
            j = matij->GetId();
            rowj = row+j;
            mjj = rowj->GetData();
            matji = matij->GetReverse();
            factor = matji->GetData();
            factor /= diag;
            for(matik = rowi->GetRight(); matik != NULL; matik = matik->GetNext())
            {
                k = matik->GetId();
                rowk = row+k;
                mkk = rowk->GetData();
                val = factor*matik->GetData();
                
                if(j == k)
                {
                    rowk->AddData(-val);
                    continue;
                }
                matjk = rowj->GetEntry(k);
                if(matjk == NULL)
                {
                    if(Abs(val) > ilut*Abs(mjj)) 
                    {
                        matjk = rowj->NewEntry(rowk);
                        if(matjk == NULL) return 1;
                        matjk->AddData(-val);
                    }
                }
                else
                {
                    matjk->AddData(-val);
                }
            }
            matji->SetData(factor); 
            rowj->MoveLeft(matji);
        }
	}
	
	return 0;
}
         
void FAMGDecomp::ILUT(double *vec)
{
    /* symmetric Gauss-Seidel */

    FAMGDecompEntry *matij;
    FAMGDecompRow *rowi;
    double  sum, *v;
    int i,j;

    v = vec;
    for(i = 0; i < n; i++)
    {
        rowi = row+i;
        sum = *v;
        for(matij = rowi->GetLeft(); matij != NULL; matij = matij->GetNext())
        {
            j = matij->GetId();
            sum -= matij->GetData()*vec[j];
        }
        *v = sum;
        v++;
    }

    v = vec+(n-1);
    for(i = n-1; i >= 0; i--)
    {
        rowi = row+i;
        sum = *v;
        for(matij = rowi->GetRight(); matij != NULL; matij = matij->GetNext())
        {
            j = matij->GetId();
            sum -= matij->GetData()*vec[j];
        }
        *v = sum/rowi->GetData();
        v--;
    }
    return;
}
