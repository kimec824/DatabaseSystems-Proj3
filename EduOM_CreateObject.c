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
    objectHdr.tag=0;
    if (objHdr != NULL) {
        objectHdr.tag = objHdr->tag;
    }
    // if(objHdr==NULL){
    //     objectHdr.tag=0;
    // }
    // else objectHdr.tag=objHdr->tag;
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
    //Object 삽입을 위해 필요한 자유 공간의 크기 계산
    catpid.pageNo=catObjForFile->pageNo;
    catpid.volNo=catObjForFile->volNo;
    e=BfM_GetTrain(&catpid, (char **)&catPage, PAGE_BUF);
    if (e < 0) ERR(e);
    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);
    BfM_FreeTrain(&catpid, PAGE_BUF);

    alignedLen=ALIGNED_LENGTH(length);
    neededSpace = sizeof(ObjectHdr) + alignedLen + sizeof(SlottedPageSlot);

    pid.pageNo=catEntry->firstPage;
    pid.volNo=catEntry->fid.volNo;
    
    // e=BfM_GetTrain(&pid, (char **)&apage, PAGE_BUF);
    if (e < 0) ERR(e);
    //Object를 삽입할 page를 선정
    if(nearObj!=NULL){
    //파라미터로 주어진 nearObj가 NULL이 아닌 경우
        pid.pageNo=nearObj->pageNo;
        pid.volNo=nearObj->volNo;
        e=BfM_GetTrain(&pid,(char **)&apage, PAGE_BUF);//apage: nearObj가 들어있는 page
        if (e < 0) ERR(e);
        e = EduOM_CompactPage(apage, NIL);
            if (e < 0) ERR(e);
        if(SP_CFREE(apage)>=neededSpace){
            //nearObj가 저장된 page에 여유 공간이 있는 경우(needed space만큼의 공간이 있는 경우)
            //해당 page를 object를 삽입할 page로 선정함
            // objpage=apage;
            //선정된 page를 현재 available space list에서 삭제함
            e=om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);
            if (e < 0) ERR(e);
        }
        else{
            //nearObj가 저장된 page에 여유 공간이 없는 경우
            //새로운 page를 할당 받아 object를 삽입할 page로 선정함
            //printf("1\n");
            eff=catEntry->eff;
            e=RDsM_PageIdToExtNo(&pid, &firstExt);
            if (e < 0) ERR(e);
            // pid.pageNo = nearObj->pageNo;
            // pid.volNo = nearObj->volNo;
            e=RDsM_AllocTrains(nearObj->volNo, firstExt, &pid, eff, 1, 1, &newpid);
            //printf("1\n");
            if (e < 0) ERR(e);
            e=BfM_GetTrain(&newpid, (char **)&apage, PAGE_BUF);
            //printf("1\n");
            if (e < 0) ERR(e);
            //선정된 page의 header를 초기화함->뭘..?
            apage->header.fid=catEntry->fid;
            // objpage->header.flags=2;
            apage->header.free=0;
            apage->header.nextPage=-1;
            apage->header.nSlots=0;
            apage->header.pid=newpid;
            apage->header.prevPage=nearObj->pageNo;
            // objpage->header.prevPage=-1;
            // apage->header.reserved=0;
            apage->header.spaceListNext=-1;
            apage->header.spaceListPrev=-1;
            apage->header.unused=0;
            apage->header.unique=0;
            apage->header.uniqueLimit=0;
            apage->slot[0].offset=EMPTYSLOT;
            SET_PAGE_TYPE(apage, SLOTTED_PAGE_TYPE);
            //선정된 page를 file 구성 page들로 이루어진 list에서 nearObj가 저장된 page의 다음 page로 삽입함
            e=om_FileMapAddPage(catObjForFile, &pid, &newpid);
            //printf("1\n");
            if (e < 0) ERR(e);
            BfM_FreeTrain(&newpid, PAGE_BUF);
        }
        BfM_FreeTrain(&pid, PAGE_BUF);
    }

    else{
        // newpid.pageNo = catEntry->lastPage;
        //     newpid.volNo = catEntry->fid.volNo;
        //     e=BfM_GetTrain(&newpid,(char **)&objpage, PAGE_BUF);
        //파라미터로 주어진 nearObj가 NULL인 경우
        //Object 삽입을 위해 필요한 자유 공간의 크기에 알맞은 available space list가 존재하는 경우
            //printf("2\n");
        if(neededSpace<SP_20SIZE){
            if(catEntry->availSpaceList10!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList10;
            }
        }
        if(neededSpace<SP_30SIZE && noAvailableSpace){
            if(catEntry->availSpaceList20!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList20;
            }
        }
        if(neededSpace<SP_40SIZE && noAvailableSpace){
            if(catEntry->availSpaceList30!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList30;
            }
        }
        if(neededSpace<SP_50SIZE && noAvailableSpace){
            if(catEntry->availSpaceList40!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList40;
            }
        }
        if(noAvailableSpace){
            if(catEntry->availSpaceList50!=-1){
                noAvailableSpace = 0;
                newpid.pageNo=catEntry->availSpaceList50;
            }
        }
        newpid.volNo=catObjForFile->volNo;
        BfM_GetTrain(&newpid,(char **)&apage, PAGE_BUF);
        om_RemoveFromAvailSpaceList(catObjForFile, &newpid, apage);
        //Object 삽입을 위해 필요한 자유 공간의 크기에 알맞은 available space list가 존재하지 않는 경우
        if(noAvailableSpace){
            lastpid.pageNo=catEntry->lastPage;
            lastpid.volNo=catObjForFile->volNo;
            e=BfM_GetTrain(&lastpid, (char **)lastpage, PAGE_BUF);
            if (e < 0) ERR(e);
            e=EduOM_CompactPage(lastpage, NIL);
            if (e < 0) ERR(e);
            //file의 마지막 page에 여유 공간이 있는 경우
            if(SP_CFREE(lastpage)>=neededSpace){
                //File의 마지막 page를 object를 삽입할 page로 선정함
                apage=lastpage;
                om_RemoveFromAvailSpaceList(catObjForFile, &newpid, apage);
            }
            //file의 마지막 page에 여유 공간이 없는 경우
            else{
                //새로운 page를 할당 받아 object를 삽입할 page로 선정함->get new train 써야하나?
                eff=catEntry->eff;
                e=RDsM_PageIdToExtNo(&lastpid, &firstExt);
                if (e < 0) ERR(e);
                e=RDsM_AllocTrains(lastpid.volNo, firstExt, &lastpid, eff, 1, 1, &newpid);
                if (e < 0) ERR(e);
                // BfM_GetNewTrain()
                //선정된 page의 header를 초기화함
                apage->header.fid=catEntry->fid;
                // objpage->header.flags=2;
                apage->header.free=0;
                apage->header.nextPage=-1;
                apage->header.nSlots=0;
                apage->header.pid=newpid;
                apage->header.prevPage=nearObj->pageNo;
                // objpage->header.prevPage=-1;
                // apage->header.reserved=0;
                apage->header.spaceListNext=-1;
                apage->header.spaceListPrev=-1;
                apage->header.unused=0;
                apage->header.unique=0;
                apage->header.uniqueLimit=0;
                apage->slot[0].offset=EMPTYSLOT;
                SET_PAGE_TYPE(apage, SLOTTED_PAGE_TYPE);
                //선정된 page를 file의 구성 page들로 이루어진 list에서 마지막 page로 삽입함
                e=om_FileMapAddPage(catObjForFile, &lastpid, &newpid);
                if (e < 0) ERR(e);
            }
            BfM_FreeTrain(&lastpid, PAGE_BUF);
        }
    }
            //printf("1\n");
    //선정된 page에 object를 삽입함
    //Object의 header를 갱신함
    objHdr->length=length;
    //빈 slot 있는지 검사
    for(int i=0;i<apage->header.nSlots;i++){
        if(i!=0 && apage->slot[-i].offset==EMPTYSLOT){
            objSlot = i;
            break;
        }
    }
    //새로운 unique 만듦
    Unique *newObjUnique;
    //새로운 object 만들어서 data와 header 입력
    Object *newObject;
    //empty page의 nSlot이 초기에 1로 설정되어 있는 문제를 해결하기위한 코드
    if(apage->slot[0].offset==EMPTYSLOT)
        apage->header.nSlots=0;

            //printf("1\n");
    //빈 slot이 있는 경우: 빈 slot에 들어간다
    if(objSlot!=NULL){
        e=om_GetUnique(&(apage->header.pid), &(apage->slot[-objSlot].unique));
            //printf("1\n");
        if (e < 0) ERR(e);
        // objpage->slot[-objSlot].unique=*newObjUnique;
        apage->slot[-objSlot].offset=apage->header.free;
        oid->slotNo=objSlot;
    }
    //빈 slot이 없는 경우: 새로운 slot을 만든다
    //nslots는 처음에 1로 설정되어있다. 왜..?
    else if(objSlot==NULL){
        apage->header.nSlots++;
        oid->slotNo=apage->header.nSlots-1;
        apage->slot[-oid->slotNo].offset=apage->header.free;
        e=om_GetUnique(&(apage->header.pid), &(apage->slot[-(oid->slotNo)].unique));
        if (e < 0) ERR(e);
    }
    // objpage->header.free=&newObject;
    newObject = (Object *)&(apage->data[apage->slot[-oid->slotNo].offset]);
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
    apage->header.free+=sizeof(ObjectHdr)+alignedLen;
    //page를 알맞은 available space list에 삽입함
    e=om_PutInAvailSpaceList(catObjForFile, &(apage->header.pid), apage);
    if (e < 0) ERR(e);

    //삽입된 object의 ID를 반환함
    oid->pageNo=apage->header.pid.pageNo;
    oid->unique=apage->slot[-(oid->slotNo)].unique;
    oid->volNo=catObjForFile->volNo;
    BfM_SetDirty(&(apage->header.pid), PAGE_BUF);
    BfM_FreeTrain(&(apage->header.pid),PAGE_BUF);

    return(eNOERROR);
    
} /* eduom_CreateObject() */
