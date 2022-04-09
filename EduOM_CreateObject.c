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
 * Module : EduOM_CreateObject.c
 * 
 * Description :
 *  EduOM_CreateObject() creates a new object near the specified object.
 *
 * Exports:
 *  Four EduOM_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 */

#include <string.h>
#include "EduOM_common.h"
#include "RDsM.h"		/* for the raw disk manager call */
#include "BfM.h"		/* for the buffer manager call */
#include "EduOM_Internal.h"

/*@================================
 * EduOM_CreateObject()
 *================================*/
/*
 * Function: Four EduOM_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 * 
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 * (1) What to do?
 * EduOM_CreateObject() creates a new object near the specified object.
 * If there is no room in the page holding the specified object,
 * it trys to insert into the page in the available space list. If fail, then
 * the new object will be put into the newly allocated page.
 *
 * (2) How to do?
 *	a. Read in the near slotted page
 *	b. See the object header
 *	c. IF large object THEN
 *	       call the large object manager's lom_ReadObject()
 *	   ELSE 
 *		   IF moved object THEN 
 *				call this function recursively
 *		   ELSE 
 *				copy the data into the buffer
 *		   ENDIF
 *	   ENDIF
 *	d. Free the buffer page
 *	e. Return
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADLENGTH_OM
 *    eBADUSERBUF_OM
 *    some error codes from the lower level
 *
 * Side Effects :
 *  0) A new object is created.
 *  1) parameter oid
 *     'oid' is set to the ObjectID of the newly created object.
 */
Four EduOM_CreateObject(
    ObjectID  *catObjForFile,	/* IN file in which object is to be placed */
    ObjectID  *nearObj,		/* IN create the new object near this object */
    ObjectHdr *objHdr,		/* IN from which tag is to be set */
    Four      length,		/* IN amount of data */
    char      *data,		/* IN the initial data for the object */
    ObjectID  *oid)		/* OUT the object's ObjectID */
{
    Four        e;		/* error number */
    ObjectHdr   objectHdr;	/* ObjectHdr with tag set from parameter */


    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (length < 0) ERR(eBADLENGTH_OM);

    if (length > 0 && data == NULL) return(eBADUSERBUF_OM);

	/* Error check whether using not supported functionality by EduOM */
	if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);
    objectHdr.properties=0x0;
    objectHdr.length=0;
    if(objHdr==NULL){
        objectHdr.tag=0;
    }
    else objectHdr.tag=objHdr->tag;
    eduom_CreateObject(catObjForFile, nearObj, &objectHdr, length, data, oid);

    
    return(eNOERROR);
}

/*@================================
 * eduom_CreateObject()
 *================================*/
/*
 * Function: Four eduom_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 *
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  eduom_CreateObject() creates a new object near the specified object; the near
 *  page is the page holding the near object.
 *  If there is no room in the near page and the near object 'nearObj' is not
 *  NULL, a new page is allocated for object creation (In this case, the newly
 *  allocated page is inserted after the near page in the list of pages
 *  consiting in the file).
 *  If there is no room in the near page and the near object 'nearObj' is NULL,
 *  it trys to create a new object in the page in the available space list. If
 *  fail, then the new object will be put into the newly allocated page(In this
 *  case, the newly allocated page is appended at the tail of the list of pages
 *  cosisting in the file).
 *
 * Returns:
 *  error Code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by fuction calls
 */
Four eduom_CreateObject(
                        ObjectID	*catObjForFile,	/* IN file in which object is to be placed */
                        ObjectID 	*nearObj,	/* IN create the new object near this object */
                        ObjectHdr	*objHdr,	/* IN from which tag & properties are set */
                        Four	length,		/* IN amount of data */
                        char	*data,		/* IN the initial data for the object */
                        ObjectID	*oid)		/* OUT the object's ObjectID */
{
    Four        e;		/* error number */
    Four	neededSpace;	/* space needed to put new object [+ header] */
    SlottedPage *apage;		/* pointer to the slotted page buffer */
    Four        alignedLen;	/* aligned length of initial data */
    Boolean     needToAllocPage;/* Is there a need to alloc a new page? */
    PageID      pid;            /* PageID in which new object to be inserted */
    PageID      nearPid;
    Four        firstExt;	/* first Extent No of the file */
    Object      *obj;		/* point to the newly created object */
    Two         i;		/* index variable */
    sm_CatOverlayForData *catEntry; /* pointer to data file catalog information */
    SlottedPage *catPage;	/* pointer to buffer containing the catalog */
    FileID      fid;		/* ID of file where the new object is placed */
    Two         eff;		/* extent fill factor of file */
    Boolean     isTmp;
    PhysicalFileID pFid;
    SlottedPage *objpage;
    Four contiguouslength;
    PageID      catpid;
    PageID      newpid;
    Unique      newUnique;
    Boolean     noAvailableSpace = 1;
    PageID      lastpid;
    SlottedPage *lastpage;
    SlotNo      objSlot = NULL;
    
    
    
    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (objHdr == NULL) ERR(eBADOBJECTID_OM);
    
    /* Error check whether using not supported functionality by EduOM */
    if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);
    if(length%4==0){
        alignedLen=length;
    }if(length%4==1){
        alignedLen=length+3;
    }if(length%4==2){
        alignedLen=length+2;
    }if(length%4==3){
        alignedLen=length+1;
    }
    //Object 삽입을 위해 필요한 자유 공간의 크기 계산
    neededSpace = sizeof(ObjectHdr) + alignedLen + sizeof(SlottedPageSlot);
    catpid.pageNo=catObjForFile->pageNo;
    catpid.volNo=catObjForFile->volNo;
    BfM_GetTrain(&catpid, (char **)&catPage, PAGE_BUF);
    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);
    pid.pageNo=208;
    pid.volNo=1000;
    
    BfM_GetTrain(&pid, (char **)&apage, PAGE_BUF);
    //Object를 삽입할 page를 선정
    if(nearObj!=NULL){
    //파라미터로 주어진 nearObj가 NULL이 아닌 경우
        pid.pageNo=nearObj->pageNo;
        pid.volNo=nearObj->volNo;
        BfM_GetTrain(&pid,(char **)&apage, PAGE_BUF);
        EduOM_CompactPage(apage, NIL);
        if(SP_CFREE(apage)>=neededSpace){
            //nearObj가 저장된 page에 여유 공간이 있는 경우(needed space만큼의 공간이 있는 경우)
            //해당 page를 object를 삽입할 page로 선정함
            objpage=apage;
            //선정된 page를 현재 available space list에서 삭제함
            om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);
        }
        else{
            //nearObj가 저장된 page에 여유 공간이 없는 경우
            //새로운 page를 할당 받아 object를 삽입할 page로 선정함
            eff=catEntry->eff;
            RDsM_PageIdToExtNo(&pid, &firstExt);
            RDsM_AllocTrains(nearObj->volNo, firstExt, &pid, eff, 1, 1, &newpid);
            BfM_GetTrain(&newpid, (char **)&objpage, PAGE_BUF);
            //선정된 page의 header를 초기화함->뭘..?
            objpage->header.fid=catEntry->fid;
            objpage->header.flags=2;
            objpage->header.free=0;
            objpage->header.nextPage=-1;
            objpage->header.nSlots=0;
            objpage->header.pid=newpid;
            objpage->header.prevPage=nearObj->pageNo;
            objpage->header.reserved=0;
            objpage->header.spaceListNext=-1;
            objpage->header.spaceListPrev=-1;///////
            objpage->header.unused=0;
            // om_GetUnique(&newpid, &newUnique);
            // objpage->header.unique=newUnique;
            // objpage->header.uniqueLimit=newUnique;
            //선정된 page를 file 구성 page들로 이루어진 list에서 nearObj가 저장된 page의 다음 page로 삽입함
            om_FileMapAddPage(catObjForFile, &pid, &newpid);
        }
    }
    else{
    //파라미터로 주어진 nearObj가 NULL인 경우
        //Object 삽입을 위해 필요한 자유 공간의 크기에 알맞은 available space list가 존재하는 경우
        if(neededSpace<SP_20SIZE){
            if(catEntry->availSpaceList10!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList10;
                newpid.volNo=catpid.volNo;
                BfM_GetTrain(&newpid,(char **)&objpage, PAGE_BUF);
                om_RemoveFromAvailSpaceList(catObjForFile, &newpid, objpage);
            }
        }
        if(neededSpace<SP_30SIZE && noAvailableSpace){
            if(catEntry->availSpaceList20!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList20;
                newpid.volNo=catpid.volNo;
                BfM_GetTrain(&newpid,(char **)&objpage, PAGE_BUF);
                om_RemoveFromAvailSpaceList(catObjForFile, &newpid, objpage);
            }
        }
        if(neededSpace<SP_40SIZE && noAvailableSpace){
            if(catEntry->availSpaceList30!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList30;
                newpid.volNo=catpid.volNo;
                BfM_GetTrain(&newpid,(char **)&objpage, PAGE_BUF);
                om_RemoveFromAvailSpaceList(catObjForFile, &newpid, objpage);
            }
        }
        if(neededSpace<SP_50SIZE && noAvailableSpace){
            if(catEntry->availSpaceList40!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList40;
                newpid.volNo=catpid.volNo;
                BfM_GetTrain(&newpid,(char **)&objpage, PAGE_BUF);
                om_RemoveFromAvailSpaceList(catObjForFile, &newpid, objpage);
            }
        }
        if(noAvailableSpace){
            if(catEntry->availSpaceList50!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList50;
                newpid.volNo=catpid.volNo;
                BfM_GetTrain(&newpid,(char **)&objpage, PAGE_BUF);
                om_RemoveFromAvailSpaceList(catObjForFile, &newpid, objpage);
            }
        }
        //Object 삽입을 위해 필요한 자유 공간의 크기에 알맞은 available space list가 존재하지 않는 경우
        if(noAvailableSpace){
            lastpid.pageNo=catEntry->lastPage;
            lastpid.volNo=catpid.volNo;
            BfM_GetTrain(&lastpid, (char **)lastpage, PAGE_BUF);
            EduOM_CompactPage(lastpage, NIL);
            //file의 마지막 page에 여유 공간이 있는 경우
            if(SP_CFREE(lastpage)>=neededSpace){
                //File의 마지막 page를 object를 삽입할 page로 선정함
                objpage=lastpage;
            }
            //file의 마지막 page에 여유 공간이 없는 경우
            else{
                //새로운 page를 할당 받아 object를 삽입할 page로 선정함->get new train 써야하나?
                eff=catEntry->eff;
                RDsM_PageIdToExtNo(&lastpid, &firstExt);
                RDsM_AllocTrains(lastpid.volNo, firstExt, &lastpid, eff, 1, 1, &newpid);
                // BfM_GetNewTrain()
                //선정된 page의 header를 초기화함
                objpage->header.fid=catEntry->fid;
                objpage->header.flags=2;
                objpage->header.free=0;
                objpage->header.nextPage=-1;
                objpage->header.nSlots=0;
                objpage->header.pid=newpid;
                objpage->header.prevPage=catEntry->lastPage;
                objpage->header.reserved=0;
                objpage->header.spaceListNext=-1;
                objpage->header.spaceListPrev=-1;///////
                objpage->header.unused=0;
                // om_GetUnique(&newpid, &newUnique);
                // objpage->header.unique=newUnique;
                // objpage->header.uniqueLimit=newUnique;
                //선정된 page를 file의 구성 page들로 이루어진 list에서 마지막 page로 삽입함
                om_FileMapAddPage(catObjForFile, &lastpid, &newpid);
            }
        }
    }

    //선정된 page에 object를 삽입함
    //Object의 header를 갱신함
    objHdr->length=length;
    //빈 slot 있는지 검사
    for(int i=0;i<objpage->header.nSlots;i++){
        if(i!=0 && objpage->slot[-i].offset==EMPTYSLOT){
            objSlot = i;
            break;
        }
    }
    //새로운 unique 만듦
    Unique *newObjUnique;
    //새로운 object 만들어서 data와 header 입력
    Object *newObject;
    //empty page의 nSlot이 초기에 1로 설정되어 있는 문제를 해결하기위한 코드
    if(objpage->slot[0].offset==EMPTYSLOT)
        objpage->header.nSlots=0;

    //빈 slot이 있는 경우: 빈 slot에 들어간다
    if(objSlot!=NULL){
        om_GetUnique(objpage, &(objpage->slot[-objSlot].unique));
        // objpage->slot[-objSlot].unique=*newObjUnique;
        objpage->slot[-objSlot].offset=objpage->header.free;
        oid->slotNo=objSlot;
    }
    //빈 slot이 없는 경우: 새로운 slot을 만든다
    //nslots는 처음에 1로 설정되어있다. 왜..?
    else if(objSlot==NULL){
        objpage->header.nSlots++;
        oid->slotNo=objpage->header.nSlots-1;
        objpage->slot[-oid->slotNo].offset=objpage->header.free;
        om_GetUnique(&(objpage->header.pid), &(objpage->slot[-oid->slotNo].unique));
        // objpage->slot[-(objpage->header.nSlots-1)].unique=*newObjUnique;
        // newSlot->unique=*newObjUnique;
        // newSlot->offset=objpage->header.free;
        // objpage->slot[-(objpage->header.nSlots-1)]=*newSlot;
    }
    // objpage->header.free=&newObject;
    newObject = (Object *)&(objpage->data[objpage->slot[-oid->slotNo].offset]);
    for(int i=0;i<length;i++){
        // objpage->data[objpage->slot[-(oid->slotNo)].offset+sizeof(ObjectHdr)+i]=data[i];
        newObject->data[i]=data[i];
    }
    for(int i=length;i<alignedLen;i++){
        // objpage->data[objpage->slot[-(oid->slotNo)].offset+sizeof(ObjectHdr)+i]=NULL;
        newObject->data[i]=NULL;
    }
    // objpage->data[objpage->slot[-oid->slotNo]]=*objHdr;
    newObject->header=*objHdr;
    
    //page의 header를 갱신함
    objpage->header.free+=sizeof(ObjectHdr)+alignedLen;
    //page를 알맞은 available space list에 삽입함
    om_PutInAvailSpaceList(catObjForFile, &(objpage->header.pid), objpage);

    //삽입된 object의 ID를 반환함
    oid->pageNo=objpage->header.pid.pageNo;
    oid->unique=objpage->slot[-(oid->slotNo)].unique;
    oid->volNo=catpid.volNo;
    BfM_SetDirty(objpage, PAGE_BUF);
    BfM_FreeTrain(objpage,PAGE_BUF);
    BfM_FreeTrain(catPage, PAGE_BUF);

    return(eNOERROR);
    
} /* eduom_CreateObject() */
