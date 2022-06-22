#include<stdlib.h>
/*=전역변수===============================================================================================================*/
/* PCB */
struct KU_MMU_PCB{
	pid_t pid; //pid
    char* ptbr; // process의 page table 주소
};

/* PCB 리스트*/
//현재 존재하는 process list를 관리
typedef struct KU_MMU_NODE{
	struct KU_MMU_NODE *next;
	struct KU_MMU_PCB pcb;
}KU_MMU_NODE;
KU_MMU_NODE *ku_mmu_processList = NULL;

/* swap in되어 있는 page 리스트*/
//swap in, swap out 해줄때 사용 (FIFO기준)
typedef struct KU_MMU_SWAP_IN_PAGE{ 
    int pid; //해당 페이지의 process pid
    char* pteAddress; //해당 페이지 정보가 담겨있는 pte 주소
    struct KU_MMU_SWAP_IN_PAGE *next;
} KU_MMU_SWAP_IN_PAGE;
KU_MMU_SWAP_IN_PAGE *ku_mmu_swapIn_list = NULL;

char *ku_mmu_physical_memory; //physical memory할당
int ku_mmu_physicalSize = 0; //physical memory사이즈

char *ku_mmu_swap_space; //swap space할당
int ku_mmu_swapsize = 0; // swap space사이즈

char *ku_mmu_physical_free;
//free한 physical memory 리스트(page frame number크기의 배열 할당 -> 해당 pfn이 사용중이면 1 free하면 0)
char *ku_mmu_present_ptbr = NULL;
//현재 실행중인 process의 ptbr
/*================================================================================================================*/

/* 초기화 함수*/
// input된 사이즈로 swap space와 physical memory를 할당해줌
void *ku_mmu_init(unsigned int mem_size, unsigned int swap_size)
{
    ku_mmu_physicalSize = mem_size;
    ku_mmu_swapsize = swap_size;
    ku_mmu_physical_memory = (char *)malloc(sizeof(char) * ku_mmu_physicalSize); // 해당 사이즈 바이트 만큼 physical memory할당
    ku_mmu_swap_space=(char *)malloc(sizeof(char)*ku_mmu_swapsize); // 해당 사이즈 바이트 만큼 swap space할당

    ku_mmu_physical_free = (char *)malloc(sizeof(char) * mem_size / 4); //free한 physical memory 리스트 할당(pfn크기)

    if (ku_mmu_physical_memory == NULL || ku_mmu_swap_space == NULL){
        return NULL;
    }else{
        return ku_mmu_physical_memory;
    }
}

/*context switch 발생*/
//process list에 있는 process라면 ptbr 업데이트
//없는 Process라면 pcb와 page table을 생성, ptbr 업데이트, process list에 추가
    //8bit virtual address에서 6bit를 vpn으로 사용 -> 2^6=64개의 page table entry표현 가능 -> page table은 64바이트 크기로 할당
int ku_run_proc(char pid, char **ku_cr3)
{
    if(ku_mmu_processList==NULL){ // process list가 비어 있을때
        ku_mmu_processList = (KU_MMU_NODE *)malloc(sizeof(KU_MMU_NODE));
        ku_mmu_processList->next = NULL;
        ku_mmu_processList->pcb.pid = pid;
        ku_mmu_processList->pcb.ptbr = (char*)malloc(sizeof(char)*64); //page table 생성
        *ku_cr3 = ku_mmu_processList->pcb.ptbr; //ptbr 업데이트
        ku_mmu_present_ptbr = ku_mmu_processList->pcb.ptbr;
        return 0;
    }else{ 
        KU_MMU_NODE *ku_mmu_addNode = ku_mmu_processList;
        while (1){
            if(ku_mmu_addNode->pcb.pid==pid){ // process list에 있는 process인 경우
                *ku_cr3 = ku_mmu_addNode->pcb.ptbr; // ptbr 업데이트
                ku_mmu_present_ptbr = ku_mmu_addNode->pcb.ptbr; 
                return 0;
            }
            if(ku_mmu_addNode->next==NULL){ //process list에 없는 process인 경우
                ku_mmu_addNode->next=(KU_MMU_NODE *)malloc(sizeof(KU_MMU_NODE)); 
                ku_mmu_addNode = ku_mmu_addNode->next;
                ku_mmu_addNode->next = NULL;
                ku_mmu_addNode->pcb.pid = pid; //pcb생성
                ku_mmu_addNode->pcb.ptbr=(char*)malloc(sizeof(char)*64); //page table 생성
                *ku_cr3 = ku_mmu_addNode->pcb.ptbr; // ptbr 업그레이드
                ku_mmu_present_ptbr = ku_mmu_addNode->pcb.ptbr;
                return 0;
            }
            ku_mmu_addNode = ku_mmu_addNode->next;
        }
    }
    return -1;
}

/*Page fault발생*/
//present bit이 0이고, swap out되어 있거나/mapping되어 있지 않거나
int ku_page_fault(char pid, char va) //여긴 present bit이 0인 virtual address만 들어옴
{
    char virtual_page_number = (va & 0xFC) >> 2;
    if(virtual_page_number>64){ //접근하려는vpn 이 page table entry 의 개수보다 클때
        return -1;
    }
    
    char *page_table_entry = ku_mmu_present_ptbr + virtual_page_number; // page table entry 주소(ptbr + vpn)

    // 접근하려는 page가 swap space에 있을때 swap in 해주기 위해 swap space에서 제거
    if((*page_table_entry)>>1!=0){
        ku_mmu_swap_space[(*page_table_entry)>>1] = 0;
    }
    
    // 비어 있는 Physical memory 탐색
    for (int i = 1; i < ku_mmu_physicalSize / 4; i++){
        if (ku_mmu_physical_free[i] == 0){ //비어있는 physical memory가 있는 경우
            //pte업데이트
            *page_table_entry = i << 2; // pfn 업데이트 unused bit 0
            *page_table_entry = *page_table_entry | 0x01; // present bit 1이
            //swap in 리스트에 추가
            if(ku_mmu_swapIn_list==NULL){
                ku_mmu_swapIn_list = (KU_MMU_SWAP_IN_PAGE *)malloc(sizeof(KU_MMU_SWAP_IN_PAGE));
                ku_mmu_swapIn_list->pid = pid;
                ku_mmu_swapIn_list->pteAddress = page_table_entry;
                ku_mmu_swapIn_list->next = NULL;
            }else{
                KU_MMU_SWAP_IN_PAGE *checkPageInfo = ku_mmu_swapIn_list;
                while (1){
                    if (checkPageInfo->next == NULL){
                        checkPageInfo->next = (KU_MMU_SWAP_IN_PAGE *)malloc(sizeof(KU_MMU_SWAP_IN_PAGE));
                        checkPageInfo = checkPageInfo->next;
                        checkPageInfo->next = NULL;
                        checkPageInfo->pid = pid;
                        checkPageInfo->pteAddress = page_table_entry;
                        break;
                    }
                    checkPageInfo = checkPageInfo->next;
                }
            }
            ku_mmu_physical_free[i] = 1; //free리스트 업데이트
            return 0;
        }
    }
    //=>
    /*physical memory 모두 할당 되어 있을때*/
    //비어있는 swap space 탐색
    for (int i = 0; i < ku_mmu_swapsize;i=i+4){ //swap space도 4byte 단위(page 크기)
        //비어 있는 swap space가 있을때
        if(ku_mmu_swap_space[i]==0){ 
            KU_MMU_SWAP_IN_PAGE *swap_out_page = ku_mmu_swapIn_list; // swap in 리스트에서 FIFO원칙에 따라 swap out
            KU_MMU_SWAP_IN_PAGE *addPageInfo = ku_mmu_swapIn_list; 
            //해당 page를 swap in리스트에 업데이트 하고 physical memory 할당
            while (1){
                if(addPageInfo->next==NULL){ // physical memory를 할당받은 page list에 swap in된 page 정보 추가
                    addPageInfo->next = (KU_MMU_SWAP_IN_PAGE *)malloc(sizeof(KU_MMU_SWAP_IN_PAGE));
                    addPageInfo = addPageInfo->next;
                    addPageInfo->next = NULL;
                    addPageInfo->pid = pid;
                    *page_table_entry = *(swap_out_page->pteAddress);
                    addPageInfo->pteAddress = page_table_entry;
                    ku_mmu_swapIn_list = addPageInfo;
                    break;
                }
                addPageInfo = addPageInfo->next;
            }
            //swap out 된 페이지 pte 업데이트
            *(swap_out_page->pteAddress) = i << 1; // present bit을 0으로 만들어주고 swap space offset을 넣어줌
            *(swap_out_page->pteAddress)=*(swap_out_page->pteAddress)&0x1e;
            ku_mmu_swap_space[i] = 1; //해당 스왑 스페이스는 사용하고 있다고 업데이트
            free(swap_out_page); //free
            return 0;
        }
    }

    //=>
    /*swap space가 꽉 차 있음*/
    //해당 virtual address는 접근할 수 없음
    return -1;
}