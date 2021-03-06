//=============================================================================================================
/**
* @file     fiff_dir_tree.cpp
* @author   Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti Hamalainen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     July, 2012
*
* @section  LICENSE
*
* Copyright (C) 2012, Christoph Dinh and Matti Hamalainen. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
*       following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*       the following disclaimer in the documentation and/or other materials provided with the distribution.
*     * Neither the name of the Massachusetts General Hospital nor the names of its contributors may be used
*       to endorse or promote products derived from this software without specific prior written permission.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MASSACHUSETTS GENERAL HOSPITAL BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* @brief    Implementation of the FiffDirTree Class.
*
*/

//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "fiff_dir_tree.h"
#include "fiff_stream.h"
#include "fiff_tag.h"
//#include "fiff_ctf_comp.h"
//#include "fiff_proj.h"
//#include "fiff_info.h"


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace FIFFLIB;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

FiffDirTree::FiffDirTree()
: block(-1)
, nent(-1)
, nent_tree(-1)
, nchild(-1)
{
}


//*************************************************************************************************************

FiffDirTree::FiffDirTree(const FiffDirTree &p_FiffDirTree)
: block(p_FiffDirTree.block)
, id(p_FiffDirTree.id)
, parent_id(p_FiffDirTree.parent_id)
, dir(p_FiffDirTree.dir)
, nent(p_FiffDirTree.nent)
, nent_tree(p_FiffDirTree.nent_tree)
, children(p_FiffDirTree.children)
, nchild(p_FiffDirTree.nchild)
{

}


//*************************************************************************************************************

FiffDirTree::~FiffDirTree()
{
//    QList<FiffDirTree*>::iterator i;
//    for (i = this->children.begin(); i != this->children.end(); ++i)
//        if (*i)
//            delete *i;
}


//*************************************************************************************************************

void FiffDirTree::clear()
{
    block = -1;
    id.clear();
    parent_id.clear();
    dir.clear();
    nent = -1;
    nent_tree = -1;
    children.clear();
    nchild = -1;
}


//*************************************************************************************************************

bool FiffDirTree::copy_tree(FiffStream::SPtr p_pStreamIn, FiffId& in_id, QList<FiffDirTree>& p_Nodes, FiffStream::SPtr p_pStreamOut)
{
    if(p_Nodes.size() <= 0)
        return false;

    qint32 k, p;

    for(k = 0; k < p_Nodes.size(); ++k)
    {
        p_pStreamOut->start_block(p_Nodes[k].block);//8
        if (p_Nodes[k].id.version != -1)
        {
            if (in_id.version != -1)
                p_pStreamOut->write_id(FIFF_PARENT_FILE_ID, in_id);//9

            p_pStreamOut->write_id(FIFF_BLOCK_ID);//10
            p_pStreamOut->write_id(FIFF_PARENT_BLOCK_ID, p_Nodes[k].id);//11
        }
        for (p = 0; p < p_Nodes[k].nent; ++p)
        {
            //
            //   Do not copy these tags
            //
            if(p_Nodes[k].dir[p].kind == FIFF_BLOCK_ID || p_Nodes[k].dir[p].kind == FIFF_PARENT_BLOCK_ID || p_Nodes[k].dir[p].kind == FIFF_PARENT_FILE_ID)
                continue;

            //
            //   Read and write tags, pass data through transparently
            //
            if (!p_pStreamIn->device()->seek(p_Nodes[k].dir[p].pos)) //fseek(fidin, nodes(k).dir(p).pos, 'bof') == -1
            {
                printf("Could not seek to the tag\n");
                return false;
            }

//ToDo this is the same like read_tag
            FiffTag::SPtr tag(new FiffTag());
            //QDataStream in(fidin);
            FiffStream::SPtr in = p_pStreamIn;
            in->setByteOrder(QDataStream::BigEndian);

            //
            // Read fiff tag header from stream
            //
            *in >> tag->kind;
            *in >> tag->type;
            qint32 size;
            *in >> size;
            tag->resize(size);
            *in >> tag->next;

            //
            // Read data when available
            //
            if (tag->size() > 0)
            {
                in->readRawData(tag->data(), tag->size());
                FiffTag::convert_tag_data(tag,FIFFV_BIG_ENDIAN,FIFFV_NATIVE_ENDIAN);
            }

            //QDataStream out(p_pStreamOut);
            FiffStream::SPtr out = p_pStreamOut;
            out->setByteOrder(QDataStream::BigEndian);

            *out << (qint32)tag->kind;
            *out << (qint32)tag->type;
            *out << (qint32)tag->size();
            *out << (qint32)FIFFV_NEXT_SEQ;

            out->writeRawData(tag->data(),tag->size());
        }
        for(p = 0; p < p_Nodes[k].nchild; ++p)
        {
            QList<FiffDirTree> childList;
            childList << p_Nodes[k].children[p];
            FiffDirTree::copy_tree(p_pStreamIn, in_id, childList, p_pStreamOut);
        }
        p_pStreamOut->end_block(p_Nodes[k].block);
    }
    return true;
}


//*************************************************************************************************************

qint32 FiffDirTree::make_dir_tree(FiffStream* p_pStream, QList<FiffDirEntry>& p_Dir, FiffDirTree& p_Tree, qint32 start)
{
//    if (p_pTree != NULL)
//        delete p_pTree;
    p_Tree.clear();

    FiffTag::SPtr t_pTag;

    qint32 block;
    if(p_Dir[start].kind == FIFF_BLOCK_START)
    {
        FiffTag::read_tag(p_pStream, t_pTag, p_Dir[start].pos);
        block = *t_pTag->toInt();
    }
    else
    {
        block = 0;
    }

//    qDebug() << "start { " << p_pTree->block;

    qint32 current = start;

    p_Tree.block = block;
    p_Tree.nent = 0;
    p_Tree.nchild = 0;

    while (current < p_Dir.size())
    {
        if (p_Dir[current].kind == FIFF_BLOCK_START)
        {
            if (current != start)
            {
                FiffDirTree t_ChildTree;
                current = FiffDirTree::make_dir_tree(p_pStream,p_Dir,t_ChildTree, current);
                ++p_Tree.nchild;
                p_Tree.children.append(t_ChildTree);
            }
        }
        else if(p_Dir[current].kind == FIFF_BLOCK_END)
        {
            FiffTag::read_tag(p_pStream, t_pTag, p_Dir[start].pos);
            if (*t_pTag->toInt() == p_Tree.block)
                break;
        }
        else
        {
            ++p_Tree.nent;
            p_Tree.dir.append(p_Dir[current]);

            //
            //  Add the id information if available
            //
            if (block == 0)
            {
                if (p_Dir[current].kind == FIFF_FILE_ID)
                {
                    FiffTag::read_tag(p_pStream, t_pTag, p_Dir[current].pos);
                    p_Tree.id = t_pTag->toFiffID();
                }
            }
            else
            {
                if (p_Dir[current].kind == FIFF_BLOCK_ID)
                {
                    FiffTag::read_tag(p_pStream, t_pTag, p_Dir[current].pos);
                    p_Tree.id = t_pTag->toFiffID();
                }
                else if (p_Dir[current].kind == FIFF_PARENT_BLOCK_ID)
                {
                    FiffTag::read_tag(p_pStream, t_pTag, p_Dir[current].pos);
                    p_Tree.parent_id = t_pTag->toFiffID();
                }
            }
        }
        ++current;
    }

    //
    // Eliminate the empty directory
    //
    if(p_Tree.nent == 0)
        p_Tree.dir.clear();

//    qDebug() << "block =" << p_pTree->block << "nent =" << p_pTree->nent << "nchild =" << p_pTree->nchild;
//    qDebug() << "end } " << block;

    return current;
}


//*************************************************************************************************************

QList<FiffDirTree> FiffDirTree::dir_tree_find(fiff_int_t p_kind) const
{
    QList<FiffDirTree> nodes;
    if(this->block == p_kind)
        nodes.append(*this);

    QList<FiffDirTree>::const_iterator i;
    for (i = this->children.begin(); i != this->children.end(); ++i)
        nodes.append((*i).dir_tree_find(p_kind));

    return nodes;
}


//*************************************************************************************************************

bool FiffDirTree::find_tag(FiffStream* p_pStream, fiff_int_t findkind, FiffTag::SPtr& p_pTag) const
{
    for (qint32 p = 0; p < this->nent; ++p)
    {
       if (this->dir[p].kind == findkind)
       {
          FiffTag::read_tag(p_pStream,p_pTag,this->dir[p].pos);
          return true;
       }
    }
    if (p_pTag)
        p_pTag.clear();

    return false;
}


//*************************************************************************************************************

bool FiffDirTree::has_tag(fiff_int_t findkind)
{
    for(qint32 p = 0; p < this->nent; ++p)
        if(this->dir.at(p).kind == findkind)
            return true;
   return false;
}

