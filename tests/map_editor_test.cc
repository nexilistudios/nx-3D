#include <client/editor/editor_document.hh>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main()
{
    using namespace nx3d;
    using namespace nx3d::client::editor;
    try
    {
        MapDocument map;
        map.brushes.push_back({{0, 0, 0}, {32, 32, 32}, "blue", 8, 0, 45, "test/ramp"});
        map.props.push_back({"props/example_crate.obj", {160, 32, 0}, {32, 32, 32}, 15, true});
        std::stringstream serialized;
        writeMapDocument(serialized, map);
        require(readMapDocument(serialized) == map, "Map save loses brushes or OBJ transforms");
        std::istringstream old("nx3d-brush-map 1\nbrush 0 0 0 1 1 1\n");
        const auto legacy = readMapDocument(old);
        require(legacy.brushes.size() == 1 && legacy.props.empty(), "Legacy maps no longer load");
        for (const char* record : {
                     "# nx3d-model-v1 \"../external.obj\" 0 0 0 1 1 1 0 1",
                     "# nx3d-model-v1 \"props/crate.obj\" 0 0 0 0 1 1 0 1",
                     "# nx3d-model-v1 \"props/crate.obj\" 0 0 0 1 1 1 0 3",
                     "# nx3d-model-v1 \"props/crate.obj\" 0 0 0 1 1 1"})
        {
            std::istringstream invalid(std::string("nx3d-brush-map 2\n") + record + '\n');
            bool rejected = false;
            try
            {
                readMapDocument(invalid);
            }
            catch (const std::exception&)
            {
                rejected = true;
            }
            require(rejected, "Invalid model record was accepted");
        }
        const auto rampHit = pick(map, {0, 200, 0}, {0, -1, 0});
        require(rampHit && !rampHit->prop && rampHit->face == 1 && rampHit->normal.y > .9f,
                "Ray picking missed the top of a rotated ramp");
        const auto modelHit = pick(map, {160, 200, 0}, {0, -1, 0});
        require(modelHit && modelHit->prop && modelHit->index == 0, "Ray picking missed the OBJ bounds");
        require(!pick(map, {1000, 200, 1000}, {0, -1, 0}), "Empty space selected an object");

        Document document(map);
        require(!document.dirty(), "Fresh map is marked dirty");
        document.selection = rampHit;
        const auto before = document.data.brushes[0];
        const float bottom = before.center.y - before.halfSize.y;
        require(document.resizeFace(32), "Face extrusion failed");
        auto after = document.data.brushes[0];
        require(after.center.y - after.halfSize.y == bottom && after.halfSize.y == 48,
                "Extrusion moved the opposite face");
        require(!document.resizeFace(-1000), "Extrusion inverted the block");
        require(document.dirty() && document.undo() && document.data == map && !document.dirty(), "Undo failed");
        require(document.redo() && document.dirty(), "Redo failed");
        document.selection = modelHit;
        document.move({0, 32, 0});
        document.rotate(15);
        document.toggleSolid();
        auto prop = document.data.props[0];
        require(prop.center.y == 64 && prop.yaw == 30 && !prop.solid, "Model transform or collision edit failed");
        document.duplicate();
        require(document.data.props.size() == 2 && document.data.props[1].center.x == 192, "Model duplication failed");
        document.erase();
        require(document.data.props.size() == 1 && document.undo() && document.data.props.size() == 2,
                "Model deletion cannot be undone");
        document.markSaved();
        require(!document.dirty(), "Save marker did not clear dirty state");
        document.addBlock({320, 32, 0});
        require(document.data.brushes.back().halfSize == glm::vec3(16), "New block ignores the grid size");
        document.addProp("props/example_crate.obj", {320, 32, 64});
        require(document.selection->prop && document.data.props.back().center.z == 64, "Model placement failed");

        MapDocument cubeMap;
        cubeMap.brushes.push_back({{0, 0, 0}, {32, 32, 32}});
        Document placement(cubeMap);
        const auto center = placement.placement({0, 0, 200}, {0, 0, -1});
        require(center.z == 48 && center.x == 0 && center.y == 0,
                "New block placement intersects the selected face or ignores grid snapping");
        std::cout << "3D editor document, picking, transforms, history and OBJ records passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
