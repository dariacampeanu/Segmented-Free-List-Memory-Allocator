#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DIE(assertion, call_description)  \
	do {                                     \
		if (assertion) {                                 \
			fprintf(stderr, "(%s, %d): ", \
					__FILE__, __LINE__);  \
			perror(call_description);     \
			exit(errno);                  \
		}                                 \
	} while (0)

#define MAX_STRING_SIZE 64

typedef struct info_t info_t;
struct info_t {
	unsigned long address;
	unsigned long node_size;
	char *str;
};

typedef struct dll_node_t dll_node_t;
struct dll_node_t {
	void *data;
	dll_node_t *prev, *next;
};

typedef struct doubly_linked_list_t doubly_linked_list_t;
struct doubly_linked_list_t {
	dll_node_t *head;
	unsigned int data_size;
	unsigned int size;
};

typedef struct seg_free_list_t seg_free_list_t;
struct seg_free_list_t {
	int number_lists;
	unsigned long start_adr;
	doubly_linked_list_t **lists;
	int list_bytes;
	int type;
};

doubly_linked_list_t *
dll_create(unsigned int data_size)
{
	doubly_linked_list_t *list = malloc(sizeof(*list));
	DIE(!list, "malloc failed");

	list->head = NULL;
	list->data_size = data_size;
	list->size = 0;
	return list;
}

void sort_list(seg_free_list_t *arr)
{
	// sortarea array-ului de liste in functie de bytes
	int i, j;
	for (i = 0; i < arr->number_lists - 1; i++) {
		for (j = 0; j < arr->number_lists - i - 1; j++) {
			if (arr->lists[j]->data_size > arr->lists[j + 1]->data_size) {
				doubly_linked_list_t *temp = arr->lists[j];
				arr->lists[j] = arr->lists[j + 1];
				arr->lists[j + 1] = temp;
			}
		}
	}
}

void dll_add_nth_node(doubly_linked_list_t *list, unsigned int n,
					  unsigned long address, unsigned long node_size)
{
	dll_node_t *new_node = malloc(sizeof(*new_node));
	new_node->data = calloc(1, sizeof(info_t));

	new_node->prev = NULL;
	new_node->next = NULL;
	((info_t *)new_node->data)->address = address;
	((info_t *)new_node->data)->node_size = node_size;

	if (list->size == 0 || n == 0) {
		if (list->size == 0) {
			list->head = new_node;
		} else {
			new_node->next = list->head;
			list->head->prev = new_node;
			list->head = new_node;
		}
	} else {
		dll_node_t *current = list->head;

		for (unsigned int i = 0; i < n - 1 && current->next; i++)
			current = current->next;

		new_node->next = current->next;
		new_node->prev = current;
		if (current->next)
			current->next->prev = new_node;

		current->next = new_node;
	}

	list->size++;
}

dll_node_t *
dll_remove_nth_node(doubly_linked_list_t *list, unsigned int n)
{
	dll_node_t *current = list->head;
	dll_node_t *removed_node;

	if (n >= list->size - 1)
		n = list->size - 1;

	if (n == 0) {
		removed_node = list->head;
		list->head = current->next;
	} else {
		for (int i = 0; i < n - 1; i++)
			current = current->next;

		removed_node = current->next;
		current->next = removed_node->next;
	}

	list->size--;
	return removed_node;
}

seg_free_list_t *init_heap(int number_lists, unsigned long start_adr,
						   int list_bytes, int type)
{
	int size_list = 8; // numarul de bytes al fiecarei liste

	seg_free_list_t *arr = malloc(sizeof(*arr));
	DIE(!arr, "malloc failed");

	arr->number_lists = number_lists;
	arr->start_adr = start_adr;
	arr->list_bytes = list_bytes;
	arr->type = type;

	arr->lists = malloc(number_lists * sizeof(*arr->lists));
	DIE(!arr->lists, "malloc failed");

	for (int i = 0; i < number_lists; i++) {
		arr->lists[i] = dll_create(size_list);

		// calculez numarul de noduri per lista
		unsigned long number_nodes_per_list = list_bytes / size_list;

		for (int j = 0; j < number_nodes_per_list; j++) {
			dll_add_nth_node(arr->lists[i], j, start_adr, size_list);
			start_adr += size_list;
		}

		size_list *= 2;
	}

	return arr;
}

void put_by_address(doubly_linked_list_t *list, dll_node_t *node,
					unsigned long address, unsigned long node_size)
{ // sortez nodurile dupa adresa
	int ok = 0;
	if (list->size) {
		dll_node_t *current = list->head;
		for (int j = 0; j < list->size; j++) {
			if (((info_t *)node->data)->address <
				((info_t *)current->data)->address) {
				dll_add_nth_node(list, j, address, node_size);
				ok = 1;
				break;
			}

			current = current->next;
		}

		if (ok == 0)
			dll_add_nth_node(list, list->size, address, node_size);
	} else {
		dll_add_nth_node(list, 0, address, node_size);
	}
}

void my_malloc(int nr_bytes, int *nr_frgm,
			   doubly_linked_list_t *allocated_list,
			   seg_free_list_t *arr, int *nr_malloc)
{
	int size_arr = arr->number_lists;
	int ok = 0;
	int new_size = 0;
	unsigned long new_address = 0;

	for (int i = 0; i < size_arr; i++) {
		if (arr->lists[i]->data_size == nr_bytes && arr->lists[i]->size != 0) {
			// alocare directă a nodului potrivit
			dll_node_t *removed_node = dll_remove_nth_node(arr->lists[i], 0);
			(*nr_malloc)++;
			put_by_address(allocated_list, removed_node,
						   ((info_t *)removed_node->data)->address, nr_bytes);
			free(removed_node->data);
			free(removed_node);
			return;
		} else if (arr->lists[i]->data_size > nr_bytes &&
				 arr->lists[i]->size != 0) {
			// gestionarea fragmentării prin alocarea unei dimensiuni mai mari
			// și divizarea ei
			(*nr_frgm)++;
			(*nr_malloc)++;

			dll_node_t *removed_node = dll_remove_nth_node(arr->lists[i], 0);
			put_by_address(allocated_list, removed_node,
						   ((info_t *)removed_node->data)->address, nr_bytes);

			new_size = arr->lists[i]->data_size - nr_bytes;
			new_address = ((info_t *)removed_node->data)->address + nr_bytes;
			// verificare dacă există deja o listă de noduri
			// de dimensiunea rămasă
			for (int k = 0; k < size_arr; k++) {
				if (arr->lists[k]->data_size == new_size) {
					ok = 1;
					put_by_address(arr->lists[k], removed_node,
								   new_address, new_size);
				}
			}

			if (ok == 0) {
				// dacă nu există
				// se creează o nouă listă de dimensiune diferita(new_size)
				arr->number_lists++;
				arr->lists = realloc(arr->lists, arr->number_lists *
									 sizeof(doubly_linked_list_t *));
				arr->lists[arr->number_lists - 1] = dll_create(new_size);
				dll_add_nth_node(arr->lists[arr->number_lists - 1], 0,
								 new_address, new_size);
			}
			free(removed_node->data);
			free(removed_node);
			sort_list(arr);
			return;
		}
	}

	printf("Out of memory\n");
}

void my_free(seg_free_list_t *arr, doubly_linked_list_t *allocated_list,
			 unsigned long free_address, int *nr_free)
{
	dll_node_t *curr_node = allocated_list->head;
	int ok = 0;

	for (int i = 0; i < allocated_list->size; i++) {
		if (((info_t *)curr_node->data)->address == free_address) {
			// elibereaza nodul cu toate datele corespunzatoare
			dll_node_t *removed_node = dll_remove_nth_node(allocated_list, i);
			for (int j = 0; j < arr->number_lists; j++) {
				if (arr->lists[j]->data_size ==
						((info_t *)curr_node->data)->node_size &&
					ok == 0) {
					ok = 1;
					(*nr_free)++;
					put_by_address(arr->lists[j], removed_node,
								   ((info_t *)removed_node->data)->address,
								   ((info_t *)removed_node->data)->node_size);
				}
			}

			if (ok == 0) {
				// adauga nodul la lista de noduri libere
				(*nr_free)++;
				arr->number_lists++;
				arr->lists = realloc(arr->lists, arr->number_lists *
									 sizeof(doubly_linked_list_t *));
				arr->lists[arr->number_lists - 1] =
					dll_create(((info_t *)removed_node->data)->node_size);
				put_by_address(arr->lists[arr->number_lists - 1], removed_node,
							   ((info_t *)removed_node->data)->address,
							   ((info_t *)removed_node->data)->node_size);
			}

			if (((info_t *)removed_node->data)->str) {
				free(((info_t *)removed_node->data)->str);
				((info_t *)removed_node->data)->str = NULL;
			}
			free(removed_node->data);
			free(removed_node);
			sort_list(arr);
			return;
		}

		curr_node = curr_node->next;
	}

	printf("Invalid free\n");
}

int write_string(dll_node_t *current, char *string,
				 unsigned long start_adr_str, int number_chars)
{
	// altfel tratam cazului în care trebuie
	// să ne extindem pe mai multe blocuri
	dll_node_t *temp = current;
	unsigned long start_adr_node = ((info_t *)temp->data)->address;
	unsigned long end_adr_node = ((info_t *)temp->data)->address +
					((info_t *)temp->data)->node_size - 1;
	int index = 0;

	// verificam contiguitatea nodurilor
	while (temp->next && ((info_t *)temp->next->data)->address ==
								end_adr_node + 1) {
		end_adr_node = ((info_t *)temp->next->data)->address +
						((info_t *)temp->next->data)->node_size - 1;
		if (end_adr_node - start_adr_str + 1 >= number_chars) {
			// incepem să scriem stringul
			// până la finalul nodului curent
			dll_node_t *aux = current;
			while (number_chars > 0) {
				((info_t *)aux->data)->str =
				(char *)calloc(((info_t *)aux->data)->node_size, 1);
				if (number_chars >= ((info_t *)aux->data)->node_size) {
					memcpy(((info_t *)aux->data)->str, string + index,
						   ((info_t *)aux->data)->node_size);
				} else {
					memcpy(((info_t *)aux->data)->str,
						   string + index, number_chars);
				}

				index += ((info_t *)aux->data)->node_size;
				number_chars -= ((info_t *)aux->data)->node_size;
				aux = aux->next;
			}

			return 1;
		}

		temp = temp->next;
	}

	return 0;
}

int read_string(dll_node_t *current, unsigned long start_adr_str,
				int number_chars)
{
	// tratam cazul în care trebuie să ne extindem
	// pe mai multe blocuri
	dll_node_t *temp = current;
	unsigned long start_adr_node = ((info_t *)temp->data)->address;
	unsigned long end_adr_node = ((info_t *)temp->data)->address +
					((info_t *)temp->data)->node_size - 1;

	while (temp->next && ((info_t *)temp->next->data)->address ==
								end_adr_node + 1) {
		end_adr_node = ((info_t *)temp->next->data)->address +
						((info_t *)temp->next->data)->node_size - 1;
		if (end_adr_node - start_adr_str + 1 >= number_chars) {
			// incep sa afisez caracter cu caracter
			dll_node_t *aux = current;
			while (number_chars > 0) {
				if (number_chars > ((info_t *)aux->data)->node_size) {
					for (unsigned long i = 0; i < ((info_t *)aux->data)
						 ->node_size; i++) {
						char character = ((info_t *)aux->data)->str[i];
						if (character)
							printf("%c", character);
					}
				} else {
					for (unsigned long i = 0; i < number_chars; i++) {
						char character = ((info_t *)aux->data)->str[i];
						if (character)
							printf("%c", character);
					}
					printf("\n");
				}
				number_chars -= ((info_t *)aux->data)->node_size;
				aux = aux->next;
			}

			return 1;
		}

		temp = temp->next;
	}

	return 0;
}

int my_write(doubly_linked_list_t *allocated_list,
			 unsigned long start_adr_str, char *string, int number_chars)
{
	unsigned long start_adr_node, end_adr_node;
	dll_node_t *current = allocated_list->head;

	if (number_chars > strlen(string))
		number_chars = strlen(string);

	for (int i = 0; i < allocated_list->size; i++) {
		start_adr_node = ((info_t *)current->data)->address;
		end_adr_node = ((info_t *)current->data)->address +
					   ((info_t *)current->data)->node_size - 1;

		// verificam daca adresa de inceput a nodului
		// este egala cu adresa data de la tastatura
		if (start_adr_str == start_adr_node) {
			// scriem direct în nodul curent dacă spațiul este suficient
			if (end_adr_node - start_adr_node + 1 >= number_chars) {
				if (!((info_t *)current->data)->str) {
					((info_t *)current->data)->str =
						(char *)calloc(((info_t *)current->data)->node_size, 1);
				}
				memcpy(((info_t *)current->data)->str, string, number_chars);
				return 1;
			}

			return write_string(current, string, start_adr_str, number_chars);
		}

		current = current->next;
	}

	return 0;
}

int my_read(doubly_linked_list_t *allocated_list,
			unsigned long start_adr_str, int number_chars)
{
	unsigned long start_adr_node, end_adr_node;
	dll_node_t *current = allocated_list->head;

	for (int i = 0; i < allocated_list->size; i++) {
		start_adr_node = ((info_t *)current->data)->address;
		end_adr_node = ((info_t *)current->data)->address +
					   ((info_t *)current->data)->node_size - 1;

		if (start_adr_str >= start_adr_node && start_adr_str <= end_adr_node) {
			if (end_adr_node - start_adr_node + 1 >= number_chars) {
				// citim direct din nodul curent dacă spațiul este suficient
				for (unsigned long i = 0; i < number_chars; i++) {
					char character = ((info_t *)current->data)->str[i];
					if (character)
						printf("%c", character);
				}
				printf("\n");

				return 1;
			}

			return read_string(current, start_adr_str, number_chars);
		}

		current = current->next;
	}

	return 0;
}

int number_of_free_blocks(seg_free_list_t *arr)
{
	int nr = 0;

	for (int i = 0; i < arr->number_lists; i++)
		nr += arr->lists[i]->size;

	return nr;
}

void dump_memory(seg_free_list_t *arr, doubly_linked_list_t *allocated_list,
				 unsigned long total_memory, int nr_malloc, int nr_free,
				 int nr_frgm)
{
	printf("+++++DUMP+++++\n");
	printf("Total memory: %ld bytes\n", total_memory);
	unsigned long allocated_memory = 0;
	dll_node_t *current = allocated_list->head;

	for (int i = 0; i < allocated_list->size; i++) {
		allocated_memory += ((info_t *)current->data)->node_size;
		current = current->next;
	}

	printf("Total allocated memory: %ld bytes\n", allocated_memory);
	unsigned long free_memory = total_memory - allocated_memory;
	printf("Total free memory: %ld bytes\n", free_memory);
	printf("Free blocks: %d\n", number_of_free_blocks(arr));
	printf("Number of allocated blocks: %d\n", allocated_list->size);
	printf("Number of malloc calls: %d\n", nr_malloc);
	printf("Number of fragmentations: %d\n", nr_frgm);
	printf("Number of free calls: %d\n", nr_free);

	for (int i = 0; i < arr->number_lists; i++) {
		if (arr->lists[i]->size) {
			printf("Blocks with %d bytes - %d free block(s) : ",
				   arr->lists[i]->data_size, arr->lists[i]->size);

			dll_node_t *current = arr->lists[i]->head;

			for (int j = 0; j < arr->lists[i]->size; j++) {
				printf("0x%lx", ((info_t *)current->data)->address);
				if (j < arr->lists[i]->size - 1)
					printf(" ");

				current = current->next;
			}

			printf("\n");
		}
	}

	printf("Allocated blocks :");
	dll_node_t *allocated_node = allocated_list->head;

	while (allocated_node) {
		printf(" (0x%lx - %lu)", ((info_t *)allocated_node->data)->address,
			   ((info_t *)allocated_node->data)->node_size);
		allocated_node = allocated_node->next;
	}
	printf("\n");

	printf("-----DUMP-----\n");
}

void destroy_heap(seg_free_list_t **arr, doubly_linked_list_t *allocated_list)
{
	// eliberez memoria pentru fiecare nod din fiecare
	// listă din vectorul de liste
	for (int i = 0; i < (*arr)->number_lists; i++) {
		dll_node_t *current = (*arr)->lists[i]->head;
		while (current) {
			dll_node_t *temp = current;
			current = current->next;
			free(temp->data); // eliberez memoria pentru datele nodului
			free(temp);		  // eliberez memoria pentru nod
		}
		free((*arr)->lists[i]); // eliberez memoria pentru lista însăși
	}
	free((*arr)->lists); // eliberez memoria pentru vectorul de liste

	// eliberez memoria pentru fiecare nod din lista `allocated_list`
	dll_node_t *allocated_current = allocated_list->head;
	while (allocated_current) {
		dll_node_t *temp = allocated_current;
		allocated_current = allocated_current->next;
		if (((info_t *)temp->data)->str)
			free(((info_t *)temp->data)->str);
		free(temp->data); // eliberez memoria pentru datele nodului
		free(temp);		  // eliberez memoria pentru nod
	}
	free(allocated_list); // eliberez memoria pentru `allocated_list`

	// eliberez memoria pentru structura `seg_free_list_t`
	free(*arr);
}

int main(void)
{
	seg_free_list_t *seg_free_list;

	doubly_linked_list_t *allocated_list =
		dll_create(sizeof(doubly_linked_list_t));

	int number_lists, list_bytes, type, number_chars;
	unsigned long start_adr, start_adr_str;
	unsigned long total_memory = 0, allocated_memory = 0, free_memory = 0;
	int nr_malloc = 0, nr_free = 0, nr_frgm = 0;
	int is_valid = 1;
	char string[600], line[700];

	while (1) {
		char command[16], added_elem[MAX_STRING_SIZE];
		long nr;
		scanf("%s", command);

		if (strncmp(command, "INIT_HEAP", 9) == 0 && is_valid) {
			scanf("%lx %d %d %d", &start_adr, &number_lists,
				  &list_bytes, &type);
			seg_free_list = init_heap(number_lists, start_adr,
									  list_bytes, type);
			total_memory = number_lists * list_bytes;
		} else if (strncmp(command, "MALLOC", 6) == 0 && is_valid) {
			int nr_bytes;
			scanf(" %d", &nr_bytes);
			my_malloc(nr_bytes, &nr_frgm, allocated_list,
					  seg_free_list, &nr_malloc);
		} else if (strncmp(command, "FREE", 4) == 0 && is_valid) {
			unsigned long free_address;
			scanf("%lx", &free_address);

			if (free_address == 0)
				printf("Invalid free\n");
			else
				my_free(seg_free_list, allocated_list, free_address, &nr_free);
		} else if (strncmp(command, "READ", 4) == 0 && is_valid) {
			scanf("%lx %d", &start_adr_str, &number_chars);

			if (!my_read(allocated_list, start_adr_str, number_chars)) {
				printf("Segmentation fault (core dumped)\n");
				dump_memory(seg_free_list, allocated_list, total_memory,
							nr_malloc, nr_free, nr_frgm);

				is_valid = 0;
			}
		} else if (strncmp(command, "WRITE", 5) == 0 && is_valid) {
			if (fgets(line, sizeof(line), stdin)) {
				// Utilizăm sscanf pentru a analiza conținutul liniei
				if (sscanf(line, "%lx \"%600[^\"]\" %d", &start_adr_str,
						   string, &number_chars) == 3) {
					if (!my_write(allocated_list, start_adr_str,
								  string, number_chars)) {
						printf("Segmentation fault (core dumped)\n");
						dump_memory(seg_free_list, allocated_list,
									total_memory, nr_malloc, nr_free, nr_frgm);

						is_valid = 0;
					}
				}
			}
		} else if (strncmp(command, "DUMP_MEMORY", 11) == 0 && is_valid) {
			dump_memory(seg_free_list, allocated_list, total_memory,
						nr_malloc, nr_free, nr_frgm);
		} else if (strncmp(command, "DESTROY_HEAP", 12) == 0) {
			destroy_heap(&seg_free_list, allocated_list);
			return 0;
		}
	}

	return 0;
}
