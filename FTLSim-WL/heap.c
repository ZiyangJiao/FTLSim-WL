/**
    Code for https://journaldev.com article
    Purpose: A Min Heap Implementation, representing the Binary Tree as an Array
    @author: Vijay Ramachandran
    @date: 27-02-2020
*/

//#include <stdio.h>
//#include <stdlib.h>
//#include <limits.h>
//#include "ftlsim.h"
#include "heap.h"

/***general methods***/
int parent(int i) {
    // Get the index of the parent
    return (i - 1) / 2;
}

int left_child(int i) {
    return (2*i + 1);
}

int right_child(int i) {
    return (2*i + 2);
}
/***general methods***/


/***EC min heap***/
//typedef struct ECMinHeap ECMinHeap;
//struct ECMinHeap {
//    struct segment **arr;
//    // Current Size of the Heap
//    int size;
//    // Maximum capacity of the heap
//    int capacity;
//};

struct segment *get_ecmin(ECMinHeap* heap) {
    // Return the root node element,
    // since that's the minimum
//    return heap->arr[0];
    if (heap->arr[0]->in_pool == 1){
        return heap->arr[0];
    }
    struct segment *min = NULL;
    for (int i = 1; i <= 2; i++) {
        if ((heap->arr[i]->in_pool == 1 && min == NULL) || (heap->arr[i]->in_pool == 1 && heap->arr[i]->erase_counts < min->erase_counts)) {
            min = heap->arr[i];
        }
    }
    if (min != NULL) return min;
    for (int i = 3; i <= 6; i++) {
        if ((heap->arr[i]->in_pool == 1 && min == NULL) || (heap->arr[i]->in_pool == 1 && heap->arr[i]->erase_counts < min->erase_counts)) {
            min = heap->arr[i];
        }
    }
    if (min != NULL) return min;
    return NULL;
}

ECMinHeap* init_ecminheap(int capacity) {
    ECMinHeap* minheap = (ECMinHeap*) calloc (1, sizeof(ECMinHeap));
    minheap->arr = (struct segment**) calloc (capacity, sizeof(struct segment*));
    minheap->capacity = capacity;
    minheap->size = 0;
    return minheap;
}

ECMinHeap* insert_ecminheap(ECMinHeap* heap, struct segment* element) {
    // Inserts an element to the min heap
    // We first add it to the bottom (last level)
    // of the tree, and keep swapping with it's parent
    // if it is lesser than it. We keep doing that until
    // we reach the root node. So, we will have inserted the
    // element in it's proper position to preserve the min heap property
    if (heap->size == heap->capacity) {
        fprintf(stderr, "Cannot insert. Heap is already full!\n");
        return heap;
    }
    // We can add it. Increase the size and add it to the end
    heap->size++;
    heap->arr[heap->size - 1] = element;
    element->ecmin_index = heap->size - 1;

    // Keep swapping until we reach the root
    int curr = heap->size - 1;
    // As long as you aren't in the root node, and while the
    // parent of the last element is greater than it
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts > heap->arr[curr]->erase_counts) {
        // Swap
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmin_index;
        heap->arr[parent(curr)]->ecmin_index = heap->arr[curr]->ecmin_index;
        heap->arr[curr]->ecmin_index = tmp;
        // Update the current index of element
        curr = parent(curr);
    }
    return heap;
}

ECMinHeap* ecminheapify(ECMinHeap* heap, int index) {
    // Rearranges the heap as to maintain
    // the min-heap property
    if (heap->size <= 1)
        return heap;

    int left = left_child(index);
    int right = right_child(index);

    // Variable to get the smallest element of the subtree
    // of an element an index
    int smallest = index;

    // If the left child is smaller than this element, it is
    // the smallest
    if (left < heap->size && heap->arr[left]->erase_counts < heap->arr[index]->erase_counts)
        smallest = left;

    // Similarly for the right, but we are updating the smallest element
    // so that it will definitely give the least element of the subtree
    if (right < heap->size && heap->arr[right]->erase_counts < heap->arr[smallest]->erase_counts)
        smallest = right;

    // Now if the current element is not the smallest,
    // swap with the current element. The min heap property
    // is now satisfied for this subtree. We now need to
    // recursively keep doing this until we reach the root node,
    // the point at which there will be no change!
    if (smallest != index)
    {
        struct segment* temp = heap->arr[index];
        heap->arr[index] = heap->arr[smallest];
        heap->arr[smallest] = temp;
        //swap index
        int tmp = heap->arr[index]->ecmin_index;
        heap->arr[index]->ecmin_index = heap->arr[smallest]->ecmin_index;
        heap->arr[smallest]->ecmin_index = tmp;

        heap = ecminheapify(heap, smallest);
    }

    return heap;
}

ECMinHeap* delete_ecminimum(ECMinHeap* heap) {
    // Deletes the minimum element, at the root
    if (!heap || heap->size == 0)
        return heap;

    int size = heap->size;
    struct segment* last_element = heap->arr[size-1];

    // Update root value with the last element
    heap->arr[0] = last_element;
    last_element->ecmin_index = 0;

    // Now remove the last element, by decreasing the size
    heap->size--;
    size--;

    // We need to call heapify(), to maintain the min-heap
    // property
    heap = ecminheapify(heap, 0);
    return heap;
}

ECMinHeap* ecmin_delete_element(ECMinHeap* heap, int index) {
    // Deletes an element, indexed by index
    // Ensure that it's lesser than the current root
    int cur_erase_counts = heap->arr[index]->erase_counts;
    heap->arr[index]->erase_counts = -1;

    // Now keep swapping, until we update the tree
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts > heap->arr[curr]->erase_counts) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmin_index;
        heap->arr[parent(curr)]->ecmin_index = heap->arr[curr]->ecmin_index;
        heap->arr[curr]->ecmin_index = tmp;

        curr = parent(curr);
    }

    heap->arr[0]->erase_counts = cur_erase_counts;
    // Now simply delete the minimum element
    heap = delete_ecminimum(heap);
    return heap;
}

ECMinHeap* ecmin_addone_check(ECMinHeap* heap, int index) {
    return ecminheapify(heap, index);
}

void print_ecminheap(ECMinHeap* heap) {
    // Simply print the array. This is an
    // inorder traversal of the tree
    printf("ECMin Heap:\n");
    for (int i=0; i<heap->size; i++) {
        printf("%d,%d -> ", heap->arr[i]->erase_counts, heap->arr[i]->ecmin_index);
    }
    printf("\n");
}
void free_ecminheap(ECMinHeap* heap) {
    if (!heap)
        return;
    free(heap->arr);
    free(heap);
}
/***EC min heap***/

/***EC max heap***/
//typedef struct ECMaxHeap ECMaxHeap;
//struct ECMaxHeap {
//    struct segment **arr;
//    // Current Size of the Heap
//    int size;
//    // Maximum capacity of the heap
//    int capacity;
//};

struct segment *get_ecmax(ECMaxHeap* heap) {
    struct segment *max = NULL;
    for (int i = 0; i <= 6; i++){
        if ((heap->arr[i]->in_pool == 1 && max == NULL) || (heap->arr[i]->in_pool == 1 && heap->arr[i]->erase_counts > max->erase_counts)) {
            max = heap->arr[i];
        }
        if (i == 0 || i == 2 || i == 6) {
            if (max != NULL) return max;
        }
    }
    return NULL;
}

ECMaxHeap* init_ecmaxheap(int capacity) {
    ECMaxHeap* maxheap = (ECMaxHeap*) calloc (1, sizeof(ECMaxHeap));
    maxheap->arr = (struct segment**) calloc (capacity, sizeof(struct segment*));
    maxheap->capacity = capacity;
    maxheap->size = 0;
    return maxheap;
}

ECMaxHeap* insert_ecmaxheap(ECMaxHeap* heap, struct segment* element) {
    // Inserts an element to the max heap
    // We first add it to the bottom (last level)
    // of the tree, and keep swapping with it's parent
    // if it is larger than it. We keep doing that until
    // we reach the root node. So, we will have inserted the
    // element in it's proper position to preserve the max heap property
    if (heap->size == heap->capacity) {
        fprintf(stderr, "Cannot insert. Heap is already full!\n");
        return heap;
    }
    // We can add it. Increase the size and add it to the end
    heap->size++;
    heap->arr[heap->size - 1] = element;
    element->ecmax_index = heap->size - 1;

    // Keep swapping until we reach the root
    int curr = heap->size - 1;
    // As long as you aren't in the root node, and while the
    // parent of the last element is greater than it
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts < heap->arr[curr]->erase_counts) {
        // Swap
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmax_index;
        heap->arr[parent(curr)]->ecmax_index = heap->arr[curr]->ecmax_index;
        heap->arr[curr]->ecmax_index = tmp;
        // Update the current index of element
        curr = parent(curr);
    }
    return heap;
}

ECMaxHeap* ecmaxheapify(ECMaxHeap* heap, int index) {
    // Rearranges the heap as to maintain
    // the max-heap property
    if (heap->size <= 1)
        return heap;

    int left = left_child(index);
    int right = right_child(index);

    // get the largest node
    int largest = index;

    if (left < heap->size && heap->arr[left]->erase_counts > heap->arr[index]->erase_counts)
        largest = left;

    if (right < heap->size && heap->arr[right]->erase_counts > heap->arr[largest]->erase_counts)
        largest = right;

    // Now if the current element is not the largest,
    // swap with the current element. The max heap property
    // is now satisfied for this subtree. We now need to
    // recursively keep doing this until we reach the root node,
    // the point at which there will be no change!
    if (largest != index)
    {
        struct segment* temp = heap->arr[index];
        heap->arr[index] = heap->arr[largest];
        heap->arr[largest] = temp;
        //swap index
        int tmp = heap->arr[index]->ecmax_index;
        heap->arr[index]->ecmax_index = heap->arr[largest]->ecmax_index;
        heap->arr[largest]->ecmax_index = tmp;

        heap = ecmaxheapify(heap, largest);
    }

    return heap;
}

ECMaxHeap* delete_ecmaximum(ECMaxHeap* heap) {
    // Deletes the maximum element, at the root
    if (!heap || heap->size == 0)
        return heap;

    int size = heap->size;
    struct segment* last_element = heap->arr[size-1];

    // Update root value with the last element
    heap->arr[0] = last_element;
    last_element->ecmax_index = 0;

    // Now remove the last element, by decreasing the size
    heap->size--;
    size--;

    // We need to call heapify(), to maintain the max-heap
    // property
    heap = ecmaxheapify(heap, 0);
    return heap;
}

ECMaxHeap* ecmax_delete_element(ECMaxHeap* heap, int index) {
    // Deletes an element, indexed by index
    // Ensure that it's larger than the current root
    int cur_erase_counts = heap->arr[index]->erase_counts;
    heap->arr[index]->erase_counts = INT_MAX;

    // Now keep swapping, until we update the tree
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts < heap->arr[curr]->erase_counts) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmax_index;
        heap->arr[parent(curr)]->ecmax_index = heap->arr[curr]->ecmax_index;
        heap->arr[curr]->ecmax_index = tmp;

        curr = parent(curr);
    }

    heap->arr[0]->erase_counts = cur_erase_counts;
    // Now simply delete the minimum element
    heap = delete_ecmaximum(heap);
    return heap;
}

ECMaxHeap* ecmax_addone_check(ECMaxHeap* heap, int index) {
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->erase_counts < heap->arr[curr]->erase_counts) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->ecmax_index;
        heap->arr[parent(curr)]->ecmax_index = heap->arr[curr]->ecmax_index;
        heap->arr[curr]->ecmax_index = tmp;

        curr = parent(curr);
    }
    return heap;
}

void print_ecmaxheap(ECMaxHeap* heap) {
    // Simply print the array. This is an
    // inorder traversal of the tree
    printf("ECMax Heap:\n");
    for (int i=0; i<heap->size; i++) {
        printf("%d,%d-> ", heap->arr[i]->erase_counts, heap->arr[i]->ecmax_index);
    }
    printf("\n");
}

void free_ecmaxheap(ECMaxHeap* heap) {
    if (!heap)
        return;
    free(heap->arr);
    free(heap);
}
/***EC max heap***/

/***EEC min heap***/
//typedef struct EECMinHeap EECMinHeap;
//struct EECMinHeap {
//    struct segment **arr;
//    // Current Size of the Heap
//    int size;
//    // Maximum capacity of the heap
//    int capacity;
//};

struct segment *get_eecmin(EECMinHeap* heap) {
    // Return the root node element,
    // since that's the minimum
//    return heap->arr[0];
    struct segment *min = NULL;
    for (int i = 0; i <= 6; i++){
        if ((heap->arr[i]->in_pool == 1 && min == NULL) || (heap->arr[i]->in_pool == 1 && heap->arr[i]->effective_ec < min->effective_ec)) {
            min = heap->arr[i];
        }
        if (i == 0 || i == 2 || i == 6) {
            if (min != NULL) return min;
        }
    }
    return NULL;
}

EECMinHeap* init_eecminheap(int capacity) {
    EECMinHeap* minheap = (EECMinHeap*) calloc (1, sizeof(EECMinHeap));
    minheap->arr = (struct segment**) calloc (capacity, sizeof(struct segment*));
    minheap->capacity = capacity;
    minheap->size = 0;
    return minheap;
}

EECMinHeap* insert_eecminheap(EECMinHeap* heap, struct segment* element) {
    // Inserts an element to the min heap
    // We first add it to the bottom (last level)
    // of the tree, and keep swapping with it's parent
    // if it is lesser than it. We keep doing that until
    // we reach the root node. So, we will have inserted the
    // element in it's proper position to preserve the min heap property
    if (heap->size == heap->capacity) {
        fprintf(stderr, "EECMinHeap: Cannot insert. Heap is already full!\n");
        return heap;
    }
    // We can add it. Increase the size and add it to the end
    heap->size++;
    heap->arr[heap->size - 1] = element;
    element->eecmin_index = heap->size - 1;

    // Keep swapping until we reach the root
    int curr = heap->size - 1;
    // As long as you aren't in the root node, and while the
    // parent of the last element is greater than it
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec > heap->arr[curr]->effective_ec) {
        // Swap
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmin_index;
        heap->arr[parent(curr)]->eecmin_index = heap->arr[curr]->eecmin_index;
        heap->arr[curr]->eecmin_index = tmp;
        // Update the current index of element
        curr = parent(curr);
    }
    return heap;
}

EECMinHeap* eecminheapify(EECMinHeap* heap, int index) {
    // Rearranges the heap as to maintain
    // the min-heap property
    if (heap->size <= 1)
        return heap;

    int left = left_child(index);
    int right = right_child(index);

    // Variable to get the smallest element of the subtree
    // of an element an index
    int smallest = index;

    // If the left child is smaller than this element, it is
    // the smallest
    if (left < heap->size && heap->arr[left]->effective_ec < heap->arr[index]->effective_ec)
        smallest = left;

    // Similarly for the right, but we are updating the smallest element
    // so that it will definitely give the least element of the subtree
    if (right < heap->size && heap->arr[right]->effective_ec < heap->arr[smallest]->effective_ec)
        smallest = right;

    // Now if the current element is not the smallest,
    // swap with the current element. The min heap property
    // is now satisfied for this subtree. We now need to
    // recursively keep doing this until we reach the root node,
    // the point at which there will be no change!
    if (smallest != index)
    {
        struct segment* temp = heap->arr[index];
        heap->arr[index] = heap->arr[smallest];
        heap->arr[smallest] = temp;
        //swap index
        int tmp = heap->arr[index]->eecmin_index;
        heap->arr[index]->eecmin_index = heap->arr[smallest]->eecmin_index;
        heap->arr[smallest]->eecmin_index = tmp;

        heap = eecminheapify(heap, smallest);
    }

    return heap;
}

EECMinHeap* delete_eecminimum(EECMinHeap* heap) {
    // Deletes the minimum element, at the root
    if (!heap || heap->size == 0)
        return heap;

    int size = heap->size;
    struct segment* last_element = heap->arr[size-1];

    // Update root value with the last element
    heap->arr[0] = last_element;
    last_element->eecmin_index = 0;

    // Now remove the last element, by decreasing the size
    heap->size--;
    size--;

    // We need to call heapify(), to maintain the min-heap
    // property
    heap = eecminheapify(heap, 0);
    return heap;
}

EECMinHeap* eecmin_delete_element(EECMinHeap* heap, int index) {
    // Deletes an element, indexed by index
    // Ensure that it's lesser than the current root
    int cur_erase_counts = heap->arr[index]->effective_ec;
    heap->arr[index]->effective_ec = -1;

    // Now keep swapping, until we update the tree
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec > heap->arr[curr]->effective_ec) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmin_index;
        heap->arr[parent(curr)]->eecmin_index = heap->arr[curr]->eecmin_index;
        heap->arr[curr]->eecmin_index = tmp;

        curr = parent(curr);
    }

    heap->arr[0]->effective_ec = cur_erase_counts;
    // Now simply delete the minimum element
    heap = delete_eecminimum(heap);
    return heap;
}

EECMinHeap* eecmin_addone_check(EECMinHeap* heap, int index) {
    return eecminheapify(heap, index);
}

void print_eecminheap(EECMinHeap* heap) {
    // Simply print the array. This is an
    // inorder traversal of the tree
    printf("EECMin Heap:\n");
    for (int i=0; i<heap->size; i++) {
        printf("%d,%d -> ", heap->arr[i]->effective_ec, heap->arr[i]->eecmin_index);
    }
    printf("\n");
}
void free_eecminheap(EECMinHeap* heap) {
    if (!heap)
        return;
    free(heap->arr);
    free(heap);
}
/***EC min heap***/

/***EEC max heap***/
//typedef struct EECMaxHeap EECMaxHeap;
//struct EECMaxHeap {
//    struct segment **arr;
//    // Current Size of the Heap
//    int size;
//    // Maximum capacity of the heap
//    int capacity;
//};

struct segment *get_eecmax(EECMaxHeap* heap) {
    struct segment *max = NULL;
    for (int i = 0; i <= 6; i++){
        if ((heap->arr[i]->in_pool == 1 && max == NULL) || (heap->arr[i]->in_pool == 1 && heap->arr[i]->effective_ec > max->effective_ec)) {
            max = heap->arr[i];
        }
        if (i == 0 || i == 2 || i == 6) {
            if (max != NULL) return max;
        }
    }
    return NULL;
}

EECMaxHeap* init_eecmaxheap(int capacity) {
    EECMaxHeap* maxheap = (EECMaxHeap*) calloc (1, sizeof(EECMaxHeap));
    maxheap->arr = (struct segment**) calloc (capacity, sizeof(struct segment*));
    maxheap->capacity = capacity;
    maxheap->size = 0;
    return maxheap;
}

EECMaxHeap* insert_eecmaxheap(EECMaxHeap* heap, struct segment* element) {
    // Inserts an element to the max heap
    // We first add it to the bottom (last level)
    // of the tree, and keep swapping with it's parent
    // if it is larger than it. We keep doing that until
    // we reach the root node. So, we will have inserted the
    // element in it's proper position to preserve the max heap property
    if (heap->size == heap->capacity) {
        fprintf(stderr, "Cannot insert. Heap is already full!\n");
        return heap;
    }
    // We can add it. Increase the size and add it to the end
    heap->size++;
    heap->arr[heap->size - 1] = element;
    element->eecmax_index = heap->size - 1;

    // Keep swapping until we reach the root
    int curr = heap->size - 1;
    // As long as you aren't in the root node, and while the
    // parent of the last element is greater than it
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec < heap->arr[curr]->effective_ec) {
        // Swap
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmax_index;
        heap->arr[parent(curr)]->eecmax_index = heap->arr[curr]->eecmax_index;
        heap->arr[curr]->eecmax_index = tmp;
        // Update the current index of element
        curr = parent(curr);
    }
    return heap;
}

EECMaxHeap* eecmaxheapify(EECMaxHeap* heap, int index) {
    // Rearranges the heap as to maintain
    // the max-heap property
    if (heap->size <= 1)
        return heap;

    int left = left_child(index);
    int right = right_child(index);

    // get the largest node
    int largest = index;

    if (left < heap->size && heap->arr[left]->effective_ec > heap->arr[index]->effective_ec)
        largest = left;

    if (right < heap->size && heap->arr[right]->effective_ec > heap->arr[largest]->effective_ec)
        largest = right;

    // Now if the current element is not the largest,
    // swap with the current element. The max heap property
    // is now satisfied for this subtree. We now need to
    // recursively keep doing this until we reach the root node,
    // the point at which there will be no change!
    if (largest != index)
    {
        struct segment* temp = heap->arr[index];
        heap->arr[index] = heap->arr[largest];
        heap->arr[largest] = temp;
        //swap index
        int tmp = heap->arr[index]->eecmax_index;
        heap->arr[index]->eecmax_index = heap->arr[largest]->eecmax_index;
        heap->arr[largest]->eecmax_index = tmp;

        heap = eecmaxheapify(heap, largest);
    }

    return heap;
}

EECMaxHeap* delete_eecmaximum(EECMaxHeap* heap) {
    // Deletes the maximum element, at the root
    if (!heap || heap->size == 0)
        return heap;

    int size = heap->size;
    struct segment* last_element = heap->arr[size-1];

    // Update root value with the last element
    heap->arr[0] = last_element;
    last_element->eecmax_index = 0;

    // Now remove the last element, by decreasing the size
    heap->size--;
    size--;

    // We need to call heapify(), to maintain the max-heap
    // property
    heap = eecmaxheapify(heap, 0);
    return heap;
}

EECMaxHeap* eecmax_delete_element(EECMaxHeap* heap, int index) {
    // Deletes an element, indexed by index
    // Ensure that it's larger than the current root
    int cur_erase_counts = heap->arr[index]->effective_ec;
    heap->arr[index]->effective_ec = INT_MAX;

    // Now keep swapping, until we update the tree
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec < heap->arr[curr]->effective_ec) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmax_index;
        heap->arr[parent(curr)]->eecmax_index = heap->arr[curr]->eecmax_index;
        heap->arr[curr]->eecmax_index = tmp;

        curr = parent(curr);
    }

    heap->arr[0]->effective_ec = cur_erase_counts;
    // Now simply delete the minimum element
    heap = delete_eecmaximum(heap);
    return heap;
}

EECMaxHeap* eecmax_addone_check(EECMaxHeap* heap, int index) {
    int curr = index;
    while (curr > 0 && heap->arr[parent(curr)]->effective_ec < heap->arr[curr]->effective_ec) {
        struct segment* temp = heap->arr[parent(curr)];
        heap->arr[parent(curr)] = heap->arr[curr];
        heap->arr[curr] = temp;
        //swap index
        int tmp = heap->arr[parent(curr)]->eecmax_index;
        heap->arr[parent(curr)]->eecmax_index = heap->arr[curr]->eecmax_index;
        heap->arr[curr]->eecmax_index = tmp;

        curr = parent(curr);
    }
    return heap;
}

void print_eecmaxheap(EECMaxHeap* heap) {
    // Simply print the array. This is an
    // inorder traversal of the tree
    printf("EECMax Heap:\n");
    for (int i=0; i<heap->size; i++) {
        printf("%d,%d-> ", heap->arr[i]->effective_ec, heap->arr[i]->eecmax_index);
    }
    printf("\n");
}

void free_eecmaxheap(EECMaxHeap* heap) {
    if (!heap)
        return;
    free(heap->arr);
    free(heap);
}
/***EEC max heap***/


int test() {
    /***ECMinHeap Test***/
    // Capacity of 256*1024 elements
//    ECMinHeap* heap = init_ecminheap(256*1024);
//    printf("Capacity: %d\n", heap->capacity);
//    struct segment b1;
//    b1.erase_counts = 6;
//    struct segment b2;
//    b2.erase_counts = 9;
//    struct segment b3;
//    b3.erase_counts = 10;
//    struct segment b4;
//    b4.erase_counts = 1;
//    b3.in_pool = 1;
//
//    insert_ecminheap(heap, &b1);
//    insert_ecminheap(heap, &b2);
//    insert_ecminheap(heap, &b3);
//    insert_ecminheap(heap, &b4);
//    print_ecminheap(heap);
//
//    // Delete the heap->arr[1] (50)
//    ecmin_delete_element(heap, b1.ecmin_index);
//    print_ecminheap(heap);
//    // test ecmin_addone_check
//    b3.erase_counts += 1;
//    ecmin_addone_check(heap, b3.ecmin_index);
//    print_ecminheap(heap);
//    //test get in pool min
//    printf("in pool min: %d\n", get_ecmin(heap)->erase_counts);
//
//    free_ecminheap(heap);
    /***ECMinHeap Test***/

    /***ECMaxHeap Test***/
//    // Capacity of 256*1024 elements
//    ECMaxHeap* heap = init_ecmaxheap(256*1024);
//    printf("Capacity: %d\n", heap->capacity);
//    struct segment b1;
//    b1.erase_counts = 6;
//    struct segment b2;
//    b2.erase_counts = 9;
//    struct segment b3;
//    b3.erase_counts = 9;
//    struct segment b4;
//    b4.erase_counts = 1;
//    b4.in_pool = 1;
//
//    insert_ecmaxheap(heap, &b1);
//    insert_ecmaxheap(heap, &b2);
//    insert_ecmaxheap(heap, &b3);
//    insert_ecmaxheap(heap, &b4);
//    print_ecmaxheap(heap);
//
//    // test ecmax_delete_element
//    ecmax_delete_element(heap, b1.ecmax_index);
//    print_ecmaxheap(heap);
//    printf("detele one block with ec : %d\n", b1.erase_counts);
//    // test ecmax_addone_check
//    b3.erase_counts += 1;
//    ecmax_addone_check(heap, b3.ecmax_index);
//    print_ecmaxheap(heap);
//    //test get in pool max
//    printf("in pool max: %d\n", get_ecmax(heap)->erase_counts);
//
//    free_ecmaxheap(heap);
//    /***ECMaxHeap Test***/

    /***EECMinHeap Test***/
    // Capacity of 256*1024 elements
//    EECMinHeap* heap = init_eecminheap(256*1024);
//    printf("Capacity: %d\n", heap->capacity);
//    struct segment b1;
//    b1.effective_ec = 6;
//    struct segment b2;
//    b2.effective_ec = 5;
//    struct segment b3;
//    b3.effective_ec = 10;
//    struct segment b4;
//    b4.effective_ec = 1;
//    b2.in_pool = 1;
//    b3.in_pool = 1;
//
//    insert_eecminheap(heap, &b1);
//    insert_eecminheap(heap, &b2);
//    insert_eecminheap(heap, &b3);
//    insert_eecminheap(heap, &b4);
//    print_eecminheap(heap);
//
//    // test delete
//    eecmin_delete_element(heap, get_eecmin(heap)->eecmin_index);
//    print_eecminheap(heap);
//    printf("detele one block with eec : %d\n", b2.effective_ec);
//    // test eecmin_addone_check
//    b4.effective_ec += 1;
//    eecmin_addone_check(heap, b4.eecmin_index);
//    print_eecminheap(heap);
//    //test get in pool min
//    printf("in pool min: %d\n", get_eecmin(heap)->effective_ec);
//
//    free_eecminheap(heap);
    /***EECMinHeap Test***/

    /***EECMaxHeap Test***/
    // Capacity of 256*1024 elements
    EECMaxHeap* heap = init_eecmaxheap(256*1024);
    printf("Capacity: %d\n", heap->capacity);
    struct segment b1;
    b1.effective_ec = 6;
    struct segment b2;
    b2.effective_ec = 6;
    struct segment b3;
    b3.effective_ec = 9;
    struct segment b4;
    b4.effective_ec = 1;
    b3.in_pool = 1;
    b4.in_pool = 1;
    b1.in_pool = 1;

    insert_eecmaxheap(heap, &b1);
    insert_eecmaxheap(heap, &b2);
    insert_eecmaxheap(heap, &b3);
    insert_eecmaxheap(heap, &b4);
    print_eecmaxheap(heap);

    // test eecmax_delete_element
    eecmax_delete_element(heap, get_eecmax(heap)->eecmax_index);
    print_eecmaxheap(heap);
    printf("detele one block with eec : %d\n", b3.effective_ec);
    // test eecmax_addone_check
    b2.effective_ec += 1;
    eecmax_addone_check(heap, b2.ecmax_index);
    print_eecmaxheap(heap);
    //test get in pool max
    printf("in pool max: %d\n", get_eecmax(heap)->effective_ec);

    free_eecmaxheap(heap);
    /***EECMaxHeap Test***/

    return 0;
}
