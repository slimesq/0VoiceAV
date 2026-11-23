#include <iostream>
#include <vector>
#include <memory>

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
    std::vector<std::shared_ptr<Item>> vItem;

    for(int i= 0; i < 2; i++)
    {
        std::shared_ptr<Item> item = std::make_shared<Item>();
        vItem.push_back(item);
    }

    return 0;
}