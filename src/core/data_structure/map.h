/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : map.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年1月23日
  最近修改   :
  功能描述   : simple map class for c string map to pointer
  函数列表   :
  修改历史   :
  1.日    期   : 2017年1月23日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _MATRIX_MAP_H_
#define _MATRIX_MAP_H_

#include "common.h"


#define BEFORE_START_POSITION ((POSITION)-1L)

/*
template<class ARG_KEY>
uint32_t hash_key(ARG_KEY key)
{
    ldiv_t hash_value = ldiv((long)(ARG_KEY)key, 127773);
    
    hash_value.rem = 16807 * hash_value.rem - 2836 * hash_value.quot;
    if (hash_value.rem < 0)
        hash_value.rem += 2147483647;

    return ((uint32_t)hash_value.rem);
}


template<class TYPE, class ARG_TYPE>
bool compare_element(const TYPE* element1, const ARG_TYPE*element2)
{
    return *element1 == *element2;
}

struct __PRIVATE BlockChain
{
    BlockChain *m_next;             //point to next bytes block

    void *get_data() {return this + 1;}

    static BlockChain * create(BlockChain * &current_head, uint32_t max, uint32_t element_size)
    {
        if (0 == max || 0 == element_size)
        {
            return NULL;
        }

        BlockChain *ptr = (BlockChain *)new uint8_t[sizeof(BlockChain) + max * element_size];

        //insert into front
        ptr->m_next = current_head;
        current_head = ptr;

        return ptr;
    }

    void free_chain()
    {
        BlockChain * ptr = this;

        while(ptr)
        {
            uint8_t * bytes = (uint8_t *)ptr;
            BlockChain *next = ptr->m_next;

            //release mem
            delete [] bytes;
            
            ptr = next;
        }
    }
};*/


template<class KEY, class ARG_KEY, class VALUE, class ARG_VALUE>
class  __EXPORT map
{
protected:

    class map_node
    {
    public:
        map_node(ARG_KEY key) : m_key(key), m_next(NULL) {}
     
        uint32_t m_hash_value;

        const KEY m_key;
        VALUE  m_value;
        
        map_node *m_next;

        //map is friend class of map_node
        friend class map<KEY, ARG_KEY, VALUE, ARG_VALUE>;
    };

public:

    enum
    {
        DEFAULT_BLOCK_SIZE = 2,                         //default bytes block element size
        DEFAULT_HASH_TABLE_BUCKET_SIZE = 17             //default hash table bucket size
    };

    explicit map(/*uint32_t block_size = DEFAULT_BLOCK_SIZE, */ uint32_t bucket_size = DEFAULT_HASH_TABLE_BUCKET_SIZE)
    {
        m_size = 0;
        
        m_bucket = NULL;
        m_bucket_size = bucket_size;

        //m_free_list = NULL;
        
        //m_block_chain = NULL;
        //m_block_size = block_size;
    }

    virtual ~map()
    {
        remove_all();
    }

    void init_hash_table(bool alloc_now = true)
    {
        if (NULL != m_bucket)
        {
            delete []m_bucket;
            m_bucket = NULL;
        }

        m_size = 0;

        if (alloc_now)
        {
            m_bucket = new map_node*[m_bucket_size];
            memset(m_bucket, 0x00, sizeof(map_node *) * m_bucket_size);
        }
    }

    uint32_t size() const {return m_size;}

    uint32_t get_bucket_size() const {return m_bucket_size;}

    bool empty() const {return 0 == m_size;}

    bool find(ARG_KEY key, VALUE& value)
    {
        uint32_t bucket, hash_value;

        //get map node
        map_node * node = get_map_node(key, bucket, hash_value);

        if (NULL == node)
        {
            return false;
        }
        else
        {
            value = node->m_value;
            return true;
        }
    }

    VALUE& operator[](ARG_KEY key)
    {
        uint32_t bucket, hash_value;
        map_node *node = get_map_node(key, bucket, hash_value);

        if (NULL == node)
        {
            if (NULL == m_bucket)
            {
                init_hash_table(m_bucket_size);
            }

            node = new_map_node(key);
            node->m_hash_value = hash_value;

            node->m_next = m_bucket[bucket];
            m_bucket[bucket] = node;
        }

        return node->m_value;
    }
    
    void set_at(ARG_KEY key, ARG_VALUE new_value)
    {
        (*this)[key] = new_value;
    }

    bool remove_key(ARG_KEY key)
    {
        if (NULL == m_bucket)
        {
            return false;
        }

        uint32_t hash_value;
        map_node ** p_pre_node = NULL;

        //pointer to bucket
        hash_value = hash_key(key);
        p_pre_node = &(m_bucket[hash_value % m_bucket_size]);

        map_node *node = *p_pre_node;
        for (; NULL != node; node = node->m_next)
        {
            if ((node->m_hash_value == hash_value) && compare_element(&node->m_key, &key))
            {
                *p_pre_node = node->m_next;
                free_map_node(node);

                return true;
            }

            p_pre_node = &node->m_next;
        }

        return false;
    }

    void remove_all()
    {
        if (NULL != m_bucket)
        {
            for (uint32_t bucket = 0; bucket < m_bucket_size; bucket++)
                {
                    map_node* node = m_bucket[bucket];
                    map_node* next_node = NULL;
                    for (; NULL != node; node = next_node)
                    {
                        next_node = node->m_next;
                        
                        //node->map_node::~map_node();
                        delete node;
                    }
                }

                // free hash table
                delete[] m_bucket;
                m_bucket = NULL;
        }

        m_size = 0;
        //m_free_list = NULL;
        //m_block_chain->free_chain();
        //m_block_chain = NULL;
    }

    POSITION get_start_position() const
    {
        return (m_size == 0) ? NULL : BEFORE_START_POSITION;
    }
    
    void get_next(POSITION& next_position, KEY& key, VALUE& value) const
    {
        map_node *node = (map_node *)next_position;

        if ((map_node *)BEFORE_START_POSITION == node)
        {
            for (uint32_t bucket = 0; bucket < m_bucket_size; bucket++)
            {
                node = m_bucket[m_bucket_size];
                if (NULL != node)
                {
                    break;
                }
            }

            //must find not null position because get_start_position return BEFORE_START_POSITION
        }

        ASSERT(NULL != node);
        map_node *next = node->m_next;
        if (NULL == next)
        {
            //find next node in bucket
            for (uint32_t bucket = ((node->m_hash_value % m_bucket_size) + 1); bucket < m_bucket_size; bucket++)
            {
                next = m_bucket[bucket];
                if (NULL != next)
                {
                    break;
                }
            }
        }

        next_position = (POSITION)next;

        key = node->m_key;
        value = node->m_value;
    }


protected:

    virtual uint32_t hash_key(ARG_KEY key)
    {
        ldiv_t hash_value = ldiv((long)(ARG_KEY)key, 127773);
        
        hash_value.rem = 16807 * hash_value.rem - 2836 * hash_value.quot;
        if (hash_value.rem < 0)
        hash_value.rem += 2147483647;

        return ((uint32_t)hash_value.rem); 
    }

    virtual bool compare_element(const KEY * element1, const ARG_KEY *element2)
    {
    	return *element1 == *element2;
    }

    map_node* new_map_node(ARG_KEY key)
    {
        m_size++;
        return new map_node(key);
    }

    /*
        map_node* new_map_node(ARG_KEY key)
        {
            if (NULL == m_free_list)
            {
                BlockChain * new_block = BlockChain::create(m_block_chain, m_block_size, sizeof(map::map_node));
                map_node *node = (map_node *)new_block->get_data();
    
                //add to free list
                node += m_block_size -1;
                for (int32_t i = (int32_t)(m_block_size - 1); i >= 0; i--, node--)
                {
                    node->m_next = m_free_list;
                    m_free_list = node;
                }
            }
    
            map_node *new_node = m_free_list;
            map_node *temp_node = new_node->m_next;              //store temporarily
    
            memset(new_node, 0x00, sizeof(map_node));
            new_node->m_next = temp_node;
    
            m_free_list = m_free_list->m_next;
            m_size++;
    
            //call construcotr
    //#pragma push_macro("new")
    //#undef new
        ::new((void *)new_node) map::map_node(key);
    //#pragma pop_macro("new")
    
        return new_node;
        }
        */
    
    
    void free_map_node(map_node * node)
    {
        //node->map_node::~map_node();
        //node->m_next = m_free_list;
        //m_free_list = node;
        
        m_size--;
        delete node;
        
        // if no more elements, cleanup completely
        if (0 == m_size)
        {
            remove_all();
        }
    }
    
    map_node* get_map_node(ARG_KEY key, uint32_t & bucket, uint32_t & hash_value)
    {
            if (NULL == m_bucket)
            {
                return NULL;
            }
            
            hash_value = hash_key(key);
            bucket = hash_value % m_bucket_size;
            
            //get the bucket
            map_node *node = m_bucket[bucket];

            //iterate the node with specific key and hash value
            for (; NULL != node; node = node->m_next)
            {
                if ((node->m_hash_value == hash_value) && compare_element(&(node->m_key), &key))
                {
                    return node;
                }
            }

            return NULL;
    }

protected:

    uint32_t m_size;                                //element size in map

    map_node** m_bucket;                     // hash table bucket 
    uint32_t  m_bucket_size;

    //BlockChain* m_block_chain;          //bytes block for all the map nodes
    //uint32_t m_block_size;

    //map_node* m_free_list;                   //free map node list for recycle

private:

    map(const map &);
    map &operator=(const map &);    
};


template<class T>
class MapStrToPtr : public map<string, char *, T *, T *>
{
public:

    MapStrToPtr() : map<string, char *, T *, T *>() {}

protected:
    
    virtual uint32_t hash_key(char * key)
    {
        uint32_t hash_value = 2166136261U;
        uint32_t first = 0;
        uint32_t last = (uint32_t)strlen(key);
        uint32_t stride = 1 + last / 10;

        for(; first < last; first += stride)
        {
            hash_value = 16777619U * hash_value ^ (uint32_t)key[first];
        }

        return hash_value;
    }
    
};


#endif /* _MATRIX_MAP_H_ */

