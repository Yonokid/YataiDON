#include "../../libs/texture.h"
#include "../../libs/text.h"
#include "../../enums.h"
#include "box_base.h"

class GenreBG {
private:
    ray::Shader shader;
    std::unique_ptr<OutlinedText> name;
    TextureIndex texture_index;
public:
    GenreBG(std::string& text_name, ray::Color color, TextureIndex texture_index);
    void update(double current_ms);
    void draw(float start_position, float end_position);
};
