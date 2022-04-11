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
 * Module: EduOM_NextObject.c
 *
 * Description:
 *  Return the next Object of the given Current Object. 
 *
 * Export:
 *  Four EduOM_NextObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 */


#include "EduOM_common.h"
#include "BfM.h"
#include "EduOM_Internal.h"

/*@================================
 * EduOM_NextObject()
 *================================*/
/*
 * Function: Four EduOM_NextObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  Return the next Object of the given Current Object.  Find the Object in the
 *  same page which has the current Object and  if there  is no next Object in
 *  the same page, find it from the next page. If the Current Object is NULL,
 *  return the first Object of the file.
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by function calls
 *
 * Side effect:
 *  1) parameter nextOID
 *     nextOID is filled with the next object's identifier
 *  2) parameter objHdr
 *     objHdr is filled with the next object's header
 */
Four EduOM_NextObject(
    ObjectID  *catObjForFile,	/* IN informations about a data file */
    ObjectID  *curOID,		/* IN a ObjectID of the current Object */
    ObjectID  *nextOID,		/* OUT the next Object of a current Object */
    ObjectHdr *objHdr)		/* OUT the object header of next object */
{
    Four e;			/* error */
    Two  i;			/* index */
    Four offset;		/* starting offset of object within a page */
    PageID pid;			/* a page identifier */
    PageNo pageNo;		/* a temporary var for next page's PageNo */
    SlottedPage *apage;		/* a pointer to the data page */
    Object *obj;		/* a pointer to the Object */
    PhysicalFileID pFid;	/* file in which the objects are located */
    SlottedPage *catPage;	/* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* data structure for catalog object access */
    PageID  nextpageID;
    SlottedPage *nextpage;
    PageID firstpid;
    PageNo firstpageNo;
    SlottedPage *firstpage;
    PageID catpid;


    /*@
     * parameter checking
     */
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (nextOID == NULL) ERR(eBADOBJECTID_OM);
    //파라미터로 주어진 curOID가 NULL인 경우
    if(curOID==NULL){
        //File의 첫번째 page의 slot array 상에서의 첫번째 object ID를 반환함
        //catpage에 대한 정보 지정
        //catObjForFile이 들어있는 page를 catPage 포인터에 입력
        //get train에서 오류발생
        catpid.pageNo=catObjForFile->pageNo;
        catpid.volNo=catObjForFile->volNo;
        BfM_GetTrain(&catpid, (char **)&catPage, PAGE_BUF);
        GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);

        //catEntry에서 첫번째 page의 pageno 찾아 firstpageNo에 할당
        firstpageNo=catEntry->firstPage;
        //pageno와 catObjForFile의 volno 이용해서 마지막 page 포인터 firstpage에 할당
        firstpid.pageNo = firstpageNo;
        firstpid.volNo = catObjForFile->volNo;
        BfM_GetTrain(&firstpid, (char **)&firstpage, PAGE_BUF);
        //해당 포인터의 첫번째 object ID를 nextOID에 입력
        nextOID->pageNo = firstpid.pageNo;
        nextOID->volNo = firstpid.volNo;
        nextOID->slotNo = 0;
        nextOID->unique = firstpage->slot[0].unique;
        objHdr=firstpage->data+firstpage->slot[0].offset;
        BfM_FreeTrain(&catpid, PAGE_BUF);
        BfM_FreeTrain(&firstpid, PAGE_BUF);
    }
    //파라미터로 주어진 curOID가 NULL이 아닌 경우    
    else{
        pid.pageNo=curOID->pageNo;
        pid.volNo=curOID->volNo;
        BfM_GetTrain(&pid,(char **)&apage,PAGE_BUF);
        //curOID에 대응하는 object를 탐색함
        obj = apage->data + apage->slot[-(curOID->slotNo)].offset;
        //Slot array 상에서, 탐색한 object의 다음 objet의 ID를 반환함
        //탐색한 object가 page의 마지막 object인 경우
        if(curOID->slotNo==apage->header.nSlots-1){
            if(apage->header.nextPage==-1){
                //탐색한 object가 file의 마지막 page의 마지막 object인 경우
                //EOS를 반환
                return EOS;
            }
            //다음 page의 첫번째 object의 ID를 반환
            //다음 page의 포인터 필요
            nextpageID.volNo=curOID->volNo;
            nextpageID.pageNo=apage->header.nextPage;
            BfM_GetTrain(&nextpageID, (char **)&nextpage, PAGE_BUF);
            nextOID->pageNo = nextpageID.pageNo;
            nextOID->volNo = nextpageID.volNo;
            nextOID->slotNo = 0;
            nextOID->unique = nextpage->slot[0].unique;
            objHdr=nextpage->data+nextpage->slot[0].offset;
            BfM_FreeTrain(&nextpageID, PAGE_BUF);
        }
        //마지막 object가 아닐 경우
        else{
            //obj(cur object)의 다음 object 반환
            nextOID->pageNo=curOID->pageNo;
            nextOID->volNo=curOID->volNo;
            nextOID->slotNo=curOID->slotNo+1;
            nextOID->unique=apage->slot[-(nextOID->slotNo)].unique;
            objHdr=apage->data+apage->slot[-(nextOID->slotNo)].offset;
        }
        BfM_FreeTrain(&pid, PAGE_BUF);
    }



    return(EOS);		/* end of scan */
    
} /* EduOM_NextObject() */
