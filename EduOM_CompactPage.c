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
 * Module : EduOM_CompactPage.c
 * 
 * Description : 
 *  EduOM_CompactPage() reorganizes the page to make sure the unused bytes
 *  in the page are located contiguously "in the middle", between the tuples
 *  and the slot array. 
 *
 * Exports:
 *  Four EduOM_CompactPage(SlottedPage*, Two)
 */


#include <string.h>
#include "EduOM_common.h"
#include "LOT.h"
#include "EduOM_Internal.h"



/*@================================
 * EduOM_CompactPage()
 *================================*/
/*
 * Function: Four EduOM_CompactPage(SlottedPage*, Two)
 * 
 * Description : 
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  (1) What to do?
 *  EduOM_CompactPage() reorganizes the page to make sure the unused bytes
 *  in the page are located contiguously "in the middle", between the tuples
 *  and the slot array. To compress out holes, objects must be moved toward
 *  the beginning of the page.
 *
 *  (2) How to do?
 *  a. Save the given page into the temporary page
 *  b. FOR each nonempty slot DO
 *	Fill the original page by copying the object from the saved page
 *          to the data area of original page pointed by 'apageDataOffset'
 *	Update the slot offset
 *	Get 'apageDataOffet' to point the next moved position
 *     ENDFOR
 *   c. Update the 'freeStart' and 'unused' field of the page
 *   d. Return
 *	
 * Returns:
 *  error code
 *    eNOERROR
 *
 * Side Effects :
 *  The slotted page is reorganized to comact the space.
 */
Four EduOM_CompactPage(
    SlottedPage	*apage,		/* IN slotted page to compact */
    Two         slotNo)		/* IN slotNo to go to the end */
{
    SlottedPage	tpage;		/* temporay page used to save the given page */
    Object *obj;		/* pointer to the object in the data area */
    Two    apageDataOffset;	/* where the next object is to be moved */
    Four   len;			/* length of object + length of ObjectHdr */
    Two    lastSlot;		/* last non empty slot */
    Two    i;			/* index variable */
    Four   datalen;
    Two    originOffset;
    Object *originObj;
    Object *prevObj;
    Two prevOffset;
    Two prevLength;
    Two myNewOffset;
    Object *me;
    
    
    apageDataOffset=0;
    len=0;
    //파라미터로 주어진 slotNo가 NIL(-1)이 아닌 경우
    if(slotNo!=NIL){
        //slotNo에 대응하는 object를 제외한 page의 모든 object들을 데이터 영역의 가장 앞부분부터 연속되게 저장함
        //i를 0부터 apage의 nSlot만큼 돌면서 앞 object의 마지막 부분으로 offset을 설정함.
        //앞 object의 마지막 부분 = sizeof(ObjectHdr) + alignedLen = len 
        for(i=0;i<apage->header.nSlots;i++){
            if(i!=slotNo){
                apage->slot[-i].offset=apageDataOffset + len;
                obj=apage->data[apage->slot[-i].offset];
                if(obj->header.length%4==0)
                    datalen=obj->header.length;
                else if(obj->header.length%4==1)
                    datalen=obj->header.length+3;
                else if(obj->header.length%4==2)
                    datalen=obj->header.length+2;
                else if(obj->header.length%4==3)
                    datalen=obj->header.length+1;
                len=sizeof(ObjectHdr)+datalen;
                apageDataOffset=apage->slot[-i].offset;
            }
            else{
                continue;
            }
            
        }
        //slotNo에 대응하는 object를 데이터 영역 상에서의 마지막 object로 저장함
        apage->slot[-slotNo].offset=apageDataOffset+len;
    }
    
    else{
        // 파라미터로 주어진 slotNo가 NIL(-1)인 경우
        // page의 모든 object들을 데이터 영역의 가장 앞부분부터 연속되게 저장함
        for(i=0;i<apage->header.nSlots;i++){
            if(apage->slot[-i].offset==EMPTYSLOT){
                printf("skip %d\n",i);
                continue;
            }
            // originOffset=apage->slot[-i].offset;
            // apage->slot[-i].offset=apageDataOffset + len;
            // // obj = (Object *)&(apage->data[apage->slot[-i].offset]);
            // // originObj = (Object *)&(apage->data[originOffset]);
            // obj = (Object *)&(apage->data[originOffset]);
            // if(obj->header.length%4==0)
            //     datalen=obj->header.length;
            // else if(obj->header.length%4==1)
            //     datalen=obj->header.length+3;
            // else if(obj->header.length%4==2)
            //     datalen=obj->header.length+2;
            // else if(obj->header.length%4==3)
            //     datalen=obj->header.length+1;
            // len=sizeof(ObjectHdr)+datalen;
            // apageDataOffset=apage->slot[-i].offset;
            Four tempOffset;
            obj = (Object *)&(apage->data[apage->slot[-i].offset]);
            tempOffset=apageDataOffset + len;
            apage->data[tempOffset]=obj;
            apage->slot[-i].offset=tempOffset;
            // apage->slot[-i].offset=apageDataOffset+len;
            // apage->data[apage->slot[-i].offset] = obj;
            printf("%d  ",i);
            printf("%d  ", apageDataOffset+len);
            printf("%s\n",apage->data[apage->slot[-i].offset]);
            if(obj->header.length%4==0)
                datalen=obj->header.length;
            else if(obj->header.length%4==1)
                datalen=obj->header.length+3;
            else if(obj->header.length%4==2)
                datalen=obj->header.length+2;
            else if(obj->header.length%4==3)
                datalen=obj->header.length+1;
            len=sizeof(ObjectHdr)+datalen;
            apageDataOffset=apage->slot[-i].offset;
        }
        // for(i=0;i<apage->header.nSlots;i++){
        //     if(apage->slot[-i].offset==EMPTYSLOT){
        //         continue;
        //     }
        //     if(i!=0){
        //         prevOffset = apage->data[apage->slot[-(i-1)].offset];
        //         prevObj = (Object *)&(prevOffset);
        //         if(prevObj->header.length%4==0)
        //             prevLength=prevObj->header.length;
        //         else if(prevObj->header.length%4==1)
        //             prevLength=prevObj->header.length+3;
        //         else if(prevObj->header.length%4==2)
        //             prevLength=prevObj->header.length+2;
        //         else if(prevObj->header.length%4==3)
        //             prevLength=prevObj->header.length+1;
        //         me = (Object *)&(apage->data[apage->slot[-i].offset]);
        //         myNewOffset=prevOffset+prevLength;
        //         apage->data[myNewOffset] = me;

        //         apage->slot[-i].offset=myNewOffset;
        //     }
        // }
    }


    //pageheader를 갱신함
    apage->header.free += apage->header.unused;
    apage->header.unused=0;

    return(eNOERROR);
    
} /* EduOM_CompactPage */
