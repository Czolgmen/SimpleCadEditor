#include <cad_core/demo/DemoScene.hpp>

#include <array>
#include <stdexcept>

namespace cad
{
    Document createDemoScene()
    {
        Document document;

        const std::array quadrilateral{
            Vec2{80, 80}, Vec2{240, 80}, Vec2{240, 200}, Vec2{80, 200}};
        const std::array triangle{
            Vec2{320, 80}, Vec2{480, 80}, Vec2{400, 220}};

        document.addPolygon(quadrilateral);
        document.addPolygon(triangle);

        if (!document.validate().empty())
            throw std::logic_error("The predefined demo scene is invalid");

        return document;
    }

} // namespace cad
