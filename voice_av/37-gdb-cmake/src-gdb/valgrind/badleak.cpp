#include <iostream>
#include <vector>

class Item
{
 public:
    Item()
    {
        printf("构造Item\n");
    }
    ~Item()
    {
        printf("析构Item\n");
    }
};


int main(void)
{
    std::vector<Item*> vItem;

    for(int i= 0; i < 2; i++)
    {
        Item *item = new Item();
        vItem.push_back(item);
    }

    return 0;
}