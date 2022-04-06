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
 * Module: EduOM_PrevObject.c
 *
 * Description: 
 *  Return the previous object of the given current object.
 *
 * Exports:
 *  Four EduOM_PrevObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 */


#include "EduOM_common.h"
#include "BfM.h"
#include "EduOM_Internal.h"

/*@================================
 * EduOM_PrevObject()
 *================================*/
/*
 * Function: Four EduOM_PrevObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 *
 * Description: 
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  Return the previous object of the given current object. Find the object in
 *  the same page which has the current object and  if there  is no previous
 *  object in the same page, find it from the previous page.
 *  If the current object is NULL, return the last object of the file.
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by function calls
 *
 * Side effect:
 *  1) parameter prevOID
 *     prevOID is filled with the previous object's identifier
 *  2) parameter objHdr
 *     objHdr is filled with the previous object's header
 */
Four EduOM_PrevObject(
    ObjectID *catObjForFile,	/* IN informations about a data file */
    ObjectID *curOID,		/* IN a ObjectID of the current object */
    ObjectID *prevOID,		/* OUT the previous object of a current object */
    ObjectHdr*objHdr)		/* OUT the object header of previous object */
{
    Four e;			/* error */
    Two  i;			/* index */
    Four offset;		/* starting offset of object within a page */
    PageID pid;			/* a page identifier */
    PageNo pageNo;		/* a temporary var for previous page's PageNo */
    SlottedPage *apage;		/* a pointer to the data page */
    Object *obj;		/* a pointer to the Object */
    SlottedPage *catPage;	/* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* overlay structure for catalog object access */
    PageID  prevpageID;
    SlottedPage *prevpage;
    PageID lastpid;
    PageNo lastpageNo;
    SlottedPage *lastpage;
    PageID catpid;


    /*@ parameter checking */
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (prevOID == NULL) ERR(eBADOBJECTID_OM);

    //파라미터로 주어진 curOID가 NULL인 경우
    if(curOID==NULL){
        //File의 마지막 page의 slot array 상에서의 마지막 object ID를 반환함
        //catpage에 대한 정보 지정
        //catObjForFile이 들어있는 page를 catPage 포인터에 입력
        //get train에서 오류발생
        catpid.pageNo=catObjForFile->pageNo;
        catpid.volNo=catObjForFile->volNo;
        BfM_GetTrain(&catpid, (char **)&catPage, PAGE_BUF);
        GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);

        //catEntry에서 마지막 page의 pageno 찾아 lastpageNo에 할당
        lastpageNo=catEntry->lastPage;
        //pageno와 catObjForFile의 volno 이용해서 마지막 page 포인터 lastpage에 할당
        lastpid.pageNo = lastpageNo;
        lastpid.volNo = catObjForFile->volNo;
        BfM_GetTrain(&lastpid, (char **)&lastpage, PAGE_BUF);
        //해당 포인터의 마지막 object ID를 prevOID에 입력
        prevOID->pageNo = lastpid.pageNo;
        prevOID->volNo = lastpid.volNo;
        prevOID->slotNo = lastpage->header.nSlots-1;
        prevOID->unique = lastpage->slot[-(prevOID->slotNo)].unique;
        // objHdr=lastpage->data + lastpage->slot[-(prevOID->slotNo)].offset;
    }
    //파라미터로 주어진 curOID가 NULL이 아닌 경우    
    else{
        pid.pageNo=curOID->pageNo;
        pid.volNo=curOID->volNo;
        BfM_GetTrain(&pid,(char **)&apage,PAGE_BUF);
        //curOID에 대응하는 object를 탐색함
        obj = apage->data + apage->slot[-(curOID->slotNo-1)].offset;
        //Slot array 상에서, 탐색한 object의 이전 objet의 ID를 반환함
        //탐색한 object가 page의 첫번째 object인 경우
        if(curOID->slotNo==0){
            if(apage->header.prevPage==-1){
                //탐색한 object가 file의 첫번째 page의 첫번째 object인 경우
                //EOS를 반환
                return EOS;
            }
            //이전 page의 마지막 object의 ID를 반환
            //이전 page의 포인터 필요
            prevpageID.volNo=curOID->volNo;
            prevpageID.pageNo=apage->header.prevPage;
            BfM_GetTrain(&prevpageID, (char **)&prevpage, PAGE_BUF);
            prevOID->pageNo = prevpageID.pageNo;
            prevOID->volNo = prevpageID.volNo;
            prevOID->slotNo = prevpage->header.nSlots-1;
            prevOID->unique = prevpage->slot[-(prevOID->slotNo)].unique;
            objHdr=prevpage->data+prevpage->slot[-(prevOID->slotNo)].offset;
        }
        //첫번째 object가 아닐 경우
        else{
            //obj(cur object)의 이전 object 반환
            prevOID->pageNo=curOID->pageNo;
            prevOID->volNo=curOID->volNo;
            prevOID->slotNo=curOID->slotNo-1;
            prevOID->unique=apage->slot[-(prevOID->slotNo)].unique;
            objHdr=apage->data+apage->slot[-(prevOID->slotNo)].offset;
        }
    }

    return(EOS);
    
} /* EduOM_PrevObject() */
