/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational-Purpose Object Storage System            */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Database and Multimedia Laboratory                                      */
/*                                                                            */
/*    Computer Science Department and                                         */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: kywhang@cs.kaist.ac.kr                                          */
/*    phone: +82-42-350-7722                                                  */
/*    fax: +82-42-350-8380                                                    */
/*                                                                            */
/*    Copyright (c) 1995-2013 by Kyu-Young Whang                              */
/*                                                                            */
/*    All rights reserved. No part of this software may be reproduced,        */
/*    stored in a retrieval system, or transmitted, in any form or by any     */
/*    means, electronic, mechanical, photocopying, recording, or otherwise,   */
/*    without prior written permission of the copyright owner.                */
/*                                                                            */
/******************************************************************************/
/*
 * Module : EduOM_DestroyObject.c
 * 
 * Description : 
 *  EduOM_DestroyObject() destroys the specified object.
 *
 * Exports:
 *  Four EduOM_DestroyObject(ObjectID*, ObjectID*, Pool*, DeallocListElem*)
 */

#include "EduOM_common.h"
#include "Util.h"		/* to get Pool */
#include "RDsM.h"
#include "BfM.h"		/* for the buffer manager call */
#include "LOT.h"		/* for the large object manager call */
#include "EduOM_Internal.h"

/*@================================
 * EduOM_DestroyObject()
 *================================*/
/*
 * Function: Four EduOM_DestroyObject(ObjectID*, ObjectID*, Pool*, DeallocListElem*)
 * 
 * Description : 
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  (1) What to do?
 *  EduOM_DestroyObject() destroys the specified object. The specified object
 *  will be removed from the slotted page. The freed space is not merged
 *  to make the contiguous space; it is done when it is needed.
 *  The page's membership to 'availSpaceList' may be changed.
 *  If the destroyed object is the only object in the page, then deallocate
 *  the page.
 *
 *  (2) How to do?
 *  a. Read in the slotted page
 *  b. Remove this page from the 'availSpaceList'
 *  c. Delete the object from the page
 *  d. Update the control information: 'unused', 'freeStart', 'slot offset'
 *  e. IF no more object in this page THEN
 *	   Remove this page from the filemap List
 *	   Dealloate this page
 *    ELSE
 *	   Put this page into the proper 'availSpaceList'
 *    ENDIF
 * f. Return
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    eBADFILEID_OM
 *    some errors caused by function calls
 */
Four EduOM_DestroyObject(
    ObjectID *catObjForFile,	/* IN file containing the object */
    ObjectID *oid,		/* IN object to destroy */
    Pool     *dlPool,		/* INOUT pool of dealloc list elements */
    DeallocListElem *dlHead)	/* INOUT head of dealloc list */
{
    Four        e;		/* error number */
    Two         i;		/* temporary variable */
    FileID      fid;		/* ID of file where the object was placed */
    PageID	    pid;		/* page on which the object resides */
    SlottedPage *apage;		/* pointer to the buffer holding the page */
    Four        offset;		/* start offset of object in data area */
    Object      *obj;		/* points to the object in data area */
    Four        alignedLen;	/* aligned length of object */
    Boolean     last;		/* indicates the object is the last one */
    SlottedPage *catPage;	/* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* overlay structure for catalog object access */
    DeallocListElem *dlElem;	/* pointer to element of dealloc list */
    PhysicalFileID pFid;	/* physical ID of file */
    
    

    /*@ Check parameters. */
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);

    if (oid == NULL) ERR(eBADOBJECTID_OM);

    pid.pageNo=oid->pageNo;
    pid.volNo=oid->volNo;
    BfM_GetTrain(&pid,(char **)&apage,PAGE_BUF);
    //삭제할 object가 저장된 page를 현재 available space list에서 삭제함
    om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);
    //삭제할 object에 대응하는 Slot을 찾음
    for(i=0;i<apage->header.nSlots;i++){
        if(apage->slot[-i].unique==oid->unique){
            offset=apage->slot[-i].offset;
            break;
        }
    }
    //object 시작 포인터 + offset 가 obj
    //alignedLen
    obj = apage->data+offset;
    if(obj->header.length%4==0){
        alignedLen=obj->header.length;
    }
    if(obj->header.length%4==1){
        alignedLen=obj->header.length + 3;
    }
    if(obj->header.length%4==2){
        alignedLen=obj->header.length + 2;
    }
    if(obj->header.length%4==3){
        alignedLen=obj->header.length + 1;
    }
    //삭제할 object에 대응하는 slot을 사용하지 않는 빈 slot으로 설정함
    apage->slot[-i].offset=EMPTYSLOT;
    //page header를 갱신함
    //마지막 slot인가?
    //-> nSlot-1 = i인가?
    if(apage->header.nSlots-1==i)
    {
        apage->header.nSlots--;
    }
    //free를 수정, 혹은 unused를 수정
    //offset + object size == free이면 free를 offset으로 수정.
    if(offset+sizeof(ObjectHdr)+alignedLen==apage->header.free){
        apage->header.free=offset;
    }
    //위의 경우 아니면 unused가 커짐.
    else{
        apage->header.unused += alignedLen + sizeof(ObjectHdr);
    }
    //삭제된 object가 page의 유일한 object이고, 해당 page가 file의 첫 번째 page가 아닌 경우
    if(apage->header.nSlots == 0 && !(apage->header.prevPage == -1)){
        //page를 file 구성 page들로 이루어진 list에서 삭제함
        om_FileMapDeletePage(catObjForFile, &pid);
        //해당 page를 deallocate함
        Util_getElementFromPool(dlPool, dlHead);
        //pFid를 구하려면 해당 page를 포함하는 file을 알아내서 첫번째 page의 pid를 알아야함.
        //slottedpagehder에서 계속 prevPage를 가서 더이상 전이 없을때까지 가면 그게 첫번째 page.
        //prevPage는 pageno만을 가지고 있는데 pageno로 slottedpage의 포인터를 찾을 수가 있나?
        //prevPage의 pageno를 찾는다. volNo는 oid의 volNo로 한다.
        //pageno, volno를 가지고 slottedpage의 pointer를 찾는다.
        //그 포인터가 가리키는 prevPage가 null인지 검사한다. 아니라면 ...반복
        //맞다면 그 slottedpage의 pageno를 pfid로 한다.
        SlottedPage *temp;
        temp=apage;
        pFid.volNo=oid->volNo;
        while(temp->header.prevPage!=-1){
            pFid.pageNo=temp->header.prevPage;
            BfM_GetTrain(&pFid, (char **)&temp, PAGE_BUF);
        }
        dlHead->type = DL_PAGE;
        dlHead->elem.pFid=pFid;
        dlHead->elem.pid=pid;
        dlHead->next=-1;
        // dlHead=dlElem;
    }
    //삭제된 object가 page의 유일한 object가 아니거나, 해당 page가 file의 첫번째 page인 경우
    else{
        om_PutInAvailSpaceList(catObjForFile, &pid, apage);
    }
    BfM_SetDirty(&pid, PAGE_BUF);
    BfM_FreeTrain(&pid, PAGE_BUF);
    
    return(eNOERROR);
    
} /* EduOM_DestroyObject() */
