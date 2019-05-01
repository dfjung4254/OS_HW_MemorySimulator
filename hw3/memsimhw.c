//
// Virual Memory Simulator Homework
// Two-level page table system
// Inverted page table with a hashing system
// Student Name: 정근화
// Student Number: B354025
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define PAGESIZEBITS 12             // 페이지 사이즈 : 4K
#define VIRTUALADDRESSBITS 32       // 가상주소공간 사이즈 : 4G

typedef struct pageTableEntry {
	int level; //(1 이나 2)
	char valid;
	//레벨1 PTE면 레벨2 PTE를 가리킨다
	struct pageTableEntry *secondLevelPageTable;
	//레벨2 PTE면 프레임번호를 가진다.
	int frameNumber;
}PageTableEntry;

typedef struct framePage {
	int number; //프레임 넘버
	int pid;    //현재 이 구간을 사용중인 프로세스 아이디
	int virtualPageNumber;  //가상페이지 넘버 VPN offset을 제외한 나머지 20bit
							//더블링크드리스트로 연결 LRU
	struct framePage *beforeLRU;
	struct framePage *nextLRU;
}FramePage;

typedef struct invertedPageTableEntry {
	int pid;
	int virtualPageNumber;
	int frameNumber;
	struct invertedPageTableEntry *next;
}InvertedPageTableEntry;

//여기다가 종합 트레이스정보를 갱신, 저장.
typedef struct processEntry {
	char *traceName;
	int pid;
	int ntraces;
	int numPageFault;
	int numPageHit;

	int num2ndLevelPageTable;

	int numIHTConflictAccess;
	int numIHTNULLAccess;
	int numIHTNonNULLAccess;

	PageTableEntry *firstLevelPageTable;
	FILE *tracefp;
}ProcessEntry;

FramePage *oldestFrame;
//프로그램 input 인자들.
int firstLevelBits, phyMemSizeBits, numProcess;

void initPhyMem(FramePage *phyMem, int nFrame) {
	for (int i = 0; i < nFrame; i++) {
		phyMem[i].number = i;
		phyMem[i].pid = -1;
		phyMem[i].virtualPageNumber = -1;
		phyMem[i].beforeLRU = &phyMem[(i - 1 + nFrame) % nFrame];
		phyMem[i].nextLRU = &phyMem[(i + 1 + nFrame) % nFrame];
	}
	oldestFrame = &phyMem[0];
}

void secondLevelVMSim(ProcessEntry *procTable, FramePage *phyMemFrames) {
	for (int i = 0; i < numProcess; i++) {
		printf("**** %s *****\n", procTable[i].traceName);
		printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n", i, procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void invertedPageVMSim(ProcessEntry *procTable, FramePage *phyMemFrames, int nFrame) {
	for (int i = 0; i < numProcess; i++) {
		printf("**** %s *****\n", procTable[i].traceName);
		printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n", i, procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n", i, procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n", i, procTable[i].numIHTNonNULLAccess);
		printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAccess == procTable[i].ntraces);
	}
}

int main(int argc, char *argv[]) {
	//프로그램 예외케이스
	if (argc < 4) {
		//인자를 모두 입력하지 않음
		//최소 firstLevelBits와 phyMemSizeBits 와 트레이스할 파일 해서 3개는 입력해야함
		printf("Usage : %s firstLevelBits PhysicalMemorySizeBits TraceFileNames\n", argv[0]);
		exit(1);
	}

	//초기변수 입력.
	firstLevelBits = atoi(argv[1]);
	phyMemSizeBits = atoi(argv[2]);
	numProcess = argc - 3;

	//잘못된 실행입력 예외케이스
	if (phyMemSizeBits < PAGESIZEBITS) {
		//물리 메모리사이즈는 페이지사이즈 비트로 정한 12비트보다는 크거나 같아야함
		printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits %d\n", phyMemSizeBits, PAGESIZEBITS);
		exit(1);
	}
	if (VIRTUALADDRESSBITS - PAGESIZEBITS - firstLevelBits <= 0) {
		//가상주소공간은 32비트인데, 최소한 페이지사이즈 비트로정한 12비트 + 1레벨 페이지사이즈 보다는 커야함
		//따라서 만약 첫번째 input 값에 20이 넘는 숫자를 입력한다면 예외발생
		printf("firstLevelBits %d is too Big\n", firstLevelBits);
		exit(1);
	}

	//프로세스 테이블 시작
	//초기화
	FILE *fp[999];
	ProcessEntry *procTable = (ProcessEntry*)malloc(sizeof(ProcessEntry)*numProcess);
	for (int i = 0; i < numProcess; i++) {
		printf("process %d opening %s\n", i, argv[i + 3]);
		fp[i] = fopen(argv[i + 3], "r");
		procTable[i].traceName = (char*)malloc(sizeof(char) * 100);
		strcpy(procTable[i].traceName, argv[i + 3]);
		procTable[i].pid = i;
		procTable[i].ntraces = 0;
		procTable[i].numPageFault = 0;
		procTable[i].numPageHit = 0;
		procTable[i].num2ndLevelPageTable = 0;
		procTable[i].numIHTConflictAccess = 0;
		procTable[i].numIHTNULLAccess = 0;
		procTable[i].numIHTNonNULLAccess = 0;
		procTable[i].tracefp = fp[i];
		//firstLevelPageTable의 개수는 2^firstLevelBits 개.
		//firstLevelPageTable 은 10비트를 쓰고 secondLevelPageTable 은 10비트를쓴다.
		//나머지 12비트는 PAGESIZEBITS 인 12비트 총 32비트로 가상메모리 구현
		//모두 초기화해서 생성해준다.
		int nFirstPage = 1 << firstLevelBits;
		procTable[i].firstLevelPageTable = (PageTableEntry*)malloc(sizeof(PageTableEntry)*(nFirstPage));
		for (int j = 0; j < nFirstPage; j++) {
			procTable[i].firstLevelPageTable[j].level = 1;
			procTable[i].firstLevelPageTable[j].valid = 0;
			procTable[i].firstLevelPageTable[j].frameNumber = -1;
			procTable[i].firstLevelPageTable[j].secondLevelPageTable = NULL;
		}
	}

	//2의 제곱으로 값을 변경시키기 1 x 2^(phyMemSizeBits-PAGESIZEBITS)
	//물리사이즈는 12 부터 32까지 커버해야함.(실제로 바이트로 계산하면 2^20 에서 2^32까지임)
	//프레임의 개수임 만약 2번째 인자에 물리메모리 사이즈를 32비트로 줬다면
	//페이지 사이즈비트를 제외한 2^20개의 프레임 개수를 가질 수 있다.
	int nFrame = (1 << (phyMemSizeBits - PAGESIZEBITS));
	assert(nFrame > 0);
	printf("\nNum of Frames %d Physical Memory Size %lld bytes\n", nFrame, (1LL << phyMemSizeBits));

	//물리메모리 생성
	FramePage *phyMem = (FramePage*)malloc(sizeof(FramePage)*nFrame);
	initPhyMem(phyMem, nFrame);

	//2nd level page 시뮬레이션 시작
	printf("=============================================================\n");
	printf("The 2nd Level Page Table Memory Simulation Starts ..... \n");
	printf("=============================================================\n");
	// 이 부분에 2nd Level Page Table Memory Simulation 을 실행해야한다.
	int completeCount = 0;
	int flag[999] = { 0, };
	for (int i = 0; i < numProcess; i++) {
		while (1) {
			if (feof(fp[i])) {
				if (flag[i] == 0) {
					completeCount++;
					flag[i] = 1;
				}
				break;
			}
			unsigned addr;
			char rw;
			fscanf(fp[i], "%x %c\n", &addr, &rw);
			//읽기 테스트
			//printf("process %d %x %c\n", i, addr, rw);
			//지금 한줄 읽은 상태이다---------아래에서 시뮬레이션진행

			//10 10 12 비트로 쪼개면 안됨, firstLevelBits는 인자로 받는 값임.
			int firstPageIndex = addr >> (VIRTUALADDRESSBITS - firstLevelBits);
			int secondPageIndex = (addr - (firstPageIndex << (VIRTUALADDRESSBITS - firstLevelBits))) >> PAGESIZEBITS;
			int offset = addr - (firstPageIndex << (VIRTUALADDRESSBITS - firstLevelBits)) - (secondPageIndex << PAGESIZEBITS);
			int vpn = addr >> PAGESIZEBITS;

			//printf("%d %d %d %d %d %d %d %d\n", firstPageIndex, secondPageIndex, offset, vpn);

			// SecondPageTable이 비어있을때 초기화해줌.
			if (procTable[i].firstLevelPageTable[firstPageIndex]
				.valid == 0) {
				//printf("%d firstLevelPageTable의 secondLevelPageTable 초기화 진행\n", firstPageIndex);
				int nSecondPage = 1 << (VIRTUALADDRESSBITS - PAGESIZEBITS - firstLevelBits);
				procTable[i].firstLevelPageTable[firstPageIndex].secondLevelPageTable
					= (PageTableEntry*)malloc(sizeof(PageTableEntry)*(nSecondPage));
				for (int j = 0; j < nSecondPage; j++) {
					procTable[i].firstLevelPageTable[firstPageIndex]
						.secondLevelPageTable[j].level = 2;
					procTable[i].firstLevelPageTable[firstPageIndex]
						.secondLevelPageTable[j].valid = 0;
					procTable[i].firstLevelPageTable[firstPageIndex]
						.secondLevelPageTable[j].frameNumber = -1;
					procTable[i].firstLevelPageTable[firstPageIndex]
						.secondLevelPageTable[j].secondLevelPageTable = NULL;
				}
				procTable[i].firstLevelPageTable[firstPageIndex].valid = 1;
				procTable[i].num2ndLevelPageTable++;
			}

			//페이지 테이블 검사.
			int fn = 0;
			if (procTable[i].firstLevelPageTable[firstPageIndex]
				.secondLevelPageTable[secondPageIndex].valid == 1) {
				//page hit
				procTable[i].numPageHit++;
				//printf("페이지 히트\n" );
				//LRU 알고리즘에 의해 해당 페이지를 framePage 배열 내의
				//가장 후 순위로 이동
				//먼저 framePage 배열 중에 vpn(firstPageIndex + secondPageIndex)이
				//일치하는 framePage 찾 LinkedList의 가장 뒤로 보냄
				FramePage *searchPage = oldestFrame;
				while (searchPage) {
					if (searchPage->virtualPageNumber == vpn) {
						//해당 searchPage를 링크드리스트를 분리
						if (searchPage == oldestFrame) {
							oldestFrame = oldestFrame->nextLRU;
						}
						else {
							searchPage->beforeLRU->nextLRU = searchPage->nextLRU;
							searchPage->nextLRU->beforeLRU = searchPage->beforeLRU;
							//해당 searchPage를 후순위로 위치
							oldestFrame->beforeLRU->nextLRU = searchPage;
							searchPage->beforeLRU = oldestFrame->beforeLRU;
							oldestFrame->beforeLRU = searchPage;
							searchPage->nextLRU = oldestFrame;
						}
						break;
					}
					searchPage = searchPage->nextLRU;
				}
			}
			else {
				//page fault
				procTable[i].numPageFault++;
				//printf("페이지폴트\n");
				//LRU 알고리즘에 의해 페이지 할당
				//framePage 배열내의 가장 오래된 oldestFrame 을 제거
				//동시에 지울 protTable[i].first.second페이지의 valid를 0으로 준다.
				//새로운 프레임을 후 순위에 할당
				//동시에 생성한 protTable[i].first.second페이지의 valid를 1로 주고
				//frameNumber를 framePage의 number로 지정한다.

				//기존 테이블에 들어가서 valid를 0으로 바꿈.
				if (oldestFrame->virtualPageNumber != -1) {
					int tmpVpn = oldestFrame->virtualPageNumber;
					int tmpfirstIndex = tmpVpn >> (VIRTUALADDRESSBITS - PAGESIZEBITS) - firstLevelBits;
					int tmpsecondIndex = tmpVpn - (tmpfirstIndex << ((VIRTUALADDRESSBITS - PAGESIZEBITS) - firstLevelBits));
					procTable[oldestFrame->pid].firstLevelPageTable[tmpfirstIndex]
						.secondLevelPageTable[tmpsecondIndex].valid = 0;
				}
				//기존의 oldest는 가장 최근의 것이 되고
				//oldest의 next는 oldest가 됨. (circularList)
				oldestFrame->pid = i;   //새로 입력
				oldestFrame->virtualPageNumber = vpn; // 새로 입력
				fn = oldestFrame->number;
				oldestFrame = oldestFrame->nextLRU; // oldest
				//동시에 생성한 protTable[i].first.second페이지의 valid를 1로 주고
				//frameNumber를 framePage의 number로 지정한다.
				procTable[i].firstLevelPageTable[firstPageIndex]
					.secondLevelPageTable[secondPageIndex].valid = 1;
				procTable[i].firstLevelPageTable[firstPageIndex]
					.secondLevelPageTable[secondPageIndex].frameNumber = fn;

				//
			}
			//vpn과 offset을 그냥 더하는게 아니라 진수로 더해야함.
			unsigned physicsAddr = (procTable[i].firstLevelPageTable[firstPageIndex]
				.secondLevelPageTable[secondPageIndex].frameNumber << PAGESIZEBITS) + offset;
			procTable[i].ntraces++;
			printf("2Level procID %d traceNumber %d virtual addr %x pysical addr %x\n"
				, i, procTable[i].ntraces, addr, physicsAddr);
			break;
		}
		if (i == numProcess - 1) {
			i = -1;
		}
		if (completeCount == numProcess) {
			break;
		}
	}

	//
	// 이 부분은
	// secondLevelVMSim(); 시행
	secondLevelVMSim(procTable, phyMem);
	//

	//역태이블을 위해 invertedPageTable 을 선언해야함
	//이때까지 읽은 프로세스를 다시 되감기함.
	for (int i = 0; i < numProcess; i++) {
		rewind(procTable[i].tracefp);
		procTable[i].ntraces = 0;
		procTable[i].numPageFault = 0;
		procTable[i].numPageHit = 0;
	}
	//물리 메모리 초기화
	free(phyMem);
	phyMem = (FramePage*)malloc(sizeof(FramePage)*nFrame);
	initPhyMem(phyMem, nFrame);
	//역페이지 해시테이블 생성
	InvertedPageTableEntry *invertedPageTable
		= (InvertedPageTableEntry*)malloc(sizeof(InvertedPageTableEntry)*nFrame);
	for (int i = 0; i < nFrame; i++) {
		invertedPageTable[i].pid = -1;
		invertedPageTable[i].virtualPageNumber = -1;
		invertedPageTable[i].frameNumber = -1;
		invertedPageTable[i].next = NULL;
	}

	printf("=============================================================\n");
	printf("The Inverted Page Table Memory Simulation Starts ..... \n");
	printf("=============================================================\n");
	//이 공간에서 Inverted Page Table Memory Simulation 을 진행한다.
	completeCount = 0;
	int flag2[999] = { 0, };
	for (int i = 0; i < numProcess; i++) {
		while (1) {
			if (feof(fp[i])) {
				if (flag2[i] == 0) {
					completeCount++;
					flag2[i] = 1;
				}
				break;
			}
			unsigned addr;
			char rw;
			fscanf(fp[i], "%x %c\n", &addr, &rw);
			//읽기 테스트
			//printf("process %d %x %c\n", i, addr, rw);
			//지금 한줄 읽은 상태이다---------아래에서 시뮬레이션진행
			int vpn = 0;
			int offset = 0;
			vpn = addr >> PAGESIZEBITS;
			offset = addr - (vpn << PAGESIZEBITS);
			//비트 분리 확인
			//printf("%d %d %d\n", addr, vpn, offset);

			//해시테이블 인덱스를 구해, 해시테이블 전체 검색.
			int hashTableIndex = (vpn + i) % nFrame;
			int check = 0;
			int fn = 0;
			InvertedPageTableEntry *searchHashMap = invertedPageTable + hashTableIndex;
			if (searchHashMap->next) {
				//매핑정보 가지고 있음
				procTable[i].numIHTNonNULLAccess++;
			}
			else {
				//매핑정보 없음
				procTable[i].numIHTNULLAccess++;
			}
			while (searchHashMap) {
				//------------페이지 찾아야함, pid와 vpn이 일치하는 것.
				if (i == searchHashMap->pid
					&& searchHashMap->virtualPageNumber == vpn) {
					check = 1;
					//해당 맵의 프레임넘버 저장.
					fn = searchHashMap->frameNumber;
					//이거 정확히 무슨뜻인지 잘 모르겠다......☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆☆
					procTable[i].numIHTConflictAccess++;
					break;
				}
				searchHashMap = searchHashMap->next;
			}
			if (check == 1) {
				//페이지 히트발생
				procTable[i].numPageHit++;
				///////////////////////////////////////////

				//히트시 작동해야할 것.
				//LRU 알고리즘에 의해 해당 페이지를 framePage 배열 내의
				//가장 후 순위로 이동
				//먼저 framePage 배열 중에 vpn(firstPageIndex + secondPageIndex)이
				//일치하는 framePage 찾 LinkedList의 가장 뒤로 보냄
				FramePage *searchPage = oldestFrame;
				while (searchPage) {
					if (searchPage->virtualPageNumber == vpn) {
						if (searchPage == oldestFrame) {
							oldestFrame = oldestFrame->nextLRU;
						}
						else {
							//해당 searchPage를 링크드리스트를 분리
							searchPage->beforeLRU->nextLRU = searchPage->nextLRU;
							searchPage->nextLRU->beforeLRU = searchPage->beforeLRU;
							//해당 searchPage를 후순위로 위치
							oldestFrame->beforeLRU->nextLRU = searchPage;
							searchPage->beforeLRU = oldestFrame->beforeLRU;
							oldestFrame->beforeLRU = searchPage;
							searchPage->nextLRU = oldestFrame;
						}
						break;
					}
					searchPage = searchPage->nextLRU;
				}

			}
			else {
				//페이지 폴트발생
				procTable[i].numPageFault++;

				//기존 매핑정보 삭제(완전히 새로 메모리에 올리는 것이 아니라면)
				if (oldestFrame->virtualPageNumber != -1) {
					//이때는 해시테이블 무조건 접근해야 하는 케이스..
					int tmpPid = oldestFrame->pid;
					int tmpVPN = oldestFrame->virtualPageNumber;
					int tmpFrameNumber = oldestFrame->number;
					int tmpHashIndex = (tmpVPN + tmpPid) % nFrame;
					InvertedPageTableEntry *delHashMap = invertedPageTable + tmpHashIndex;
					InvertedPageTableEntry *beforeDelHash = NULL;
					while (delHashMap) {
						beforeDelHash = delHashMap;
						delHashMap = delHashMap->next;
						if (delHashMap->frameNumber == tmpFrameNumber) {
							//지워야 할 매핑 찾음.
							beforeDelHash->next = delHashMap->next;
							free(delHashMap);
							break;
						}
					}
				}
				//기존의 oldest는 가장 최근의 것이 되고
				//oldest의 next는 oldest가 됨. (circularList)
				oldestFrame->pid = i;   //새로 입력
				oldestFrame->virtualPageNumber = vpn; // 새로 입력
				fn = oldestFrame->number;
				oldestFrame = oldestFrame->nextLRU; // oldest
				//새로운 매핑 정보 HashTable에 저장
				InvertedPageTableEntry *initHashMap = (InvertedPageTableEntry*)malloc(sizeof(InvertedPageTableEntry));
				initHashMap->pid = i;
				initHashMap->virtualPageNumber = vpn;
				initHashMap->frameNumber = fn;
				initHashMap->next = invertedPageTable[hashTableIndex].next;
				invertedPageTable[hashTableIndex].next = initHashMap;
			}

			unsigned physicsAddr = (fn << PAGESIZEBITS) + offset;
			procTable[i].ntraces++;
			printf("IHT procID %d traceNumber %d virtual addr %x pysical addr %x\n"
				, i, procTable[i].ntraces, addr, physicsAddr);
			//
			break;
		}
		if (i == numProcess - 1) {
			i = -1;
		}
		if (completeCount == numProcess) {
			break;
		}
	}
	//
	// 아래 부분은
	// invertedPageVMSim(); 시행
	invertedPageVMSim(procTable, phyMem, nFrame);
	//
	// 파일 읽기 종료 및 free 선언
	for (int i = 0; i < numProcess; i++) {
		fclose(fp[i]);
		free(procTable[i].firstLevelPageTable);
		free(procTable[i].traceName);
	}
	free(procTable);
	free(phyMem);
	free(invertedPageTable);

	return 0;
}
