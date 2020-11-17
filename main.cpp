#include <algorithm>
#include <vector>
#include <utility>
#include <cmath>
#include <chrono>
#include <windows.h>

/* 游戏屏幕宽高。 */
#define SCR_W 120
#define SCR_H 40

/* 平面地图宽高。 */
#define MAP_W 20
#define MAP_H 15

// 平面地图模型，每个字符为一个位置单元。
const wchar_t g_map[] = L"####################"
                        L"#    #             #"
                        L"#    #             #"
                        L"#    #             #"
                        L"####               #"
                        L"#          #       #"
                        L"#                  #"
                        L"#                  #"
                        L"#      #############"
                        L"#         #        #"
                        L"#         #  ### ###"
                        L"########     #     #"
                        L"#         #  # #####"
                        L"#         #  #     E"
                        L"####################";

/* 主角在平面地图中的x、y坐标。 */
float g_playerX = 1.0f;
float g_playerY = 2.0f;
float g_playerDir = 0.0f;               // 主角在平面地图中的朝向（弧度值，0表示正右方）。
float g_playerFov = 3.1415926f / 4.0f;  // 主角的视野范围（弧度值，PI/4表示45度）。
float g_playerViewDep = 20.0f;          // 主角的视野深度（最远可视位置单元距离）。

bool g_isGameRunning = true;  // 游戏运行与否。

int main() {
    wchar_t* chars = new wchar_t[SCR_W * SCR_H];  // 根据屏幕宽高建立字符数组，每个字符在屏幕中表示一个显示单元。
    // 获得控制台屏幕缓冲区句柄。参数分别为：
    // 1、缓冲区访问权限；2、缓冲区共享权限（0表示不共享）；3、指向安全属性结构体的指针（NULL表示句柄不可被子进程继承）；
    // 4、控制台屏幕缓冲区类型（仅支持CONSOLE_TEXTMODE_BUFFER）；5、这个是设计本函数时的保留选项，当前必须为NULL。
    HANDLE console = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    SetConsoleActiveScreenBuffer(console);  // 将指定屏幕缓冲区设为当前所显示的控制台屏幕缓冲区。
    DWORD bytesWritten = 0;  // 接收将写在控制台屏幕缓冲区内的字符数组的长度。

    /* 创建2个用于计算帧间隔时间的时钟对象，并初始化为当前时间点。 */
    auto thisFrameTP = std::chrono::system_clock::now();  // 存储当前帧时间点。
    auto lastFrameTP = std::chrono::system_clock::now();  // 存储上一帧时间点。

    // 游戏主循环。
    while (g_isGameRunning) {
        thisFrameTP = std::chrono::system_clock::now();               // 更新当前帧时间点。
        std::chrono::duration<float> td = thisFrameTP - lastFrameTP;  // 计算并获得当前帧与上一帧之间以浮点秒数为单位的时间间隔对象。
        float deltaTime = td.count();                                 // 获得时间间隔对象的计次数，这里也就是间隔秒数。
        lastFrameTP = thisFrameTP;                                    // 将当前帧时间点存入上一帧时间点时钟对象。

        /******************************************************************************************
         * 处理输入。GetAsyncKeyState函数用于检测一个按键是否被按下或长按。注意使用该函数后编译时要链接user32.lib库。
         */

        if (GetAsyncKeyState(VK_LEFT))  g_playerDir -= 2.0f * deltaTime;  // 主角向左转。
        if (GetAsyncKeyState(VK_RIGHT)) g_playerDir += 2.0f * deltaTime;  // 主角向右转。
        if (GetAsyncKeyState(VK_UP)) {
            /* 主角根据朝向前进。 */
            g_playerX += cos(g_playerDir) * 3.0f * deltaTime;
            g_playerY += sin(g_playerDir) * 3.0f * deltaTime;
            // 当检测到主角前进后在平面地图中位于'#'字符处时，后退相应距离。
            if (g_map[(int)g_playerX + (int)g_playerY * MAP_W] == '#') {
                g_playerX -= cos(g_playerDir) * 3.0f * deltaTime;
                g_playerY -= sin(g_playerDir) * 3.0f * deltaTime;
            }
        }
        if (GetAsyncKeyState(VK_DOWN)) {
            /* 主角根据朝向后退。 */
            g_playerX -= cos(g_playerDir) * 3.0f * deltaTime;
            g_playerY -= sin(g_playerDir) * 3.0f * deltaTime;
            // 当检测到主角后退后在平面地图中位于'#'字符处时，前进相应距离。
            if (g_map[(int)g_playerX + (int)g_playerY * MAP_W] == '#') {
                g_playerX += cos(g_playerDir) * 3.0f * deltaTime;
                g_playerY += sin(g_playerDir) * 3.0f * deltaTime;
            }
        }

        /******************************************************************************************
         * 更新数据。
         */

        // 遍历屏幕中的每一列。
        for (int x = 0; x < SCR_W; ++x) {
            // 获得考虑主角朝向偏移后当前视野射线的弧度。
            float viewRayRad = (g_playerDir - g_playerFov * 0.5f) + (float)x / (float)SCR_W * g_playerFov;
            bool  viewRayHitWall    = false;  // 标识视野射线是否击中墙面。
            bool  viewRayHitWallCor = false;  // 标识视野射线是否击中墙的纵边。

            /* 计算当前视野射线对应单位二维向量（长度为1）的x、y值。 */
            float viewRayNorVecX = cos(viewRayRad);
            float viewRayNorVecY = sin(viewRayRad);

            float viewRayLength = 0.0f;  // 标识视野射线在平面地图中的发射长度。
            // 当视野射线尚未击中墙面并且长度未超过主角的视野深度时。
            while (!viewRayHitWall && viewRayLength < g_playerViewDep) {
                viewRayLength += 0.1f;  // 增加视野射线的长度。
                /* 在平面地图中根据主角位置和视野长度计算出改射线的x、y坐标值。 */
                int viewRayX = (int)(g_playerX + viewRayNorVecX * viewRayLength);
                int viewRayY = (int)(g_playerY + viewRayNorVecY * viewRayLength);
                // 当视野射线超出平面地图范围时。
                if (viewRayX < 0 || viewRayY < 0 || viewRayX >= MAP_W || viewRayY >= MAP_H) {
                    viewRayHitWall = true;             // 将视野射线标识为已击中墙面。
                    viewRayLength  = g_playerViewDep;  // 将视野射线长度标识为已达主角视野深度。
                } else {
                    // 若视野射线在平面地图中的位置对应字符为'#'时。
                    if (g_map[viewRayX + viewRayY * MAP_W] == '#') {
                        viewRayHitWall = true;  // 将视野射线标识为已击中墙面。

                        /* 接下来我们判断视野射线击中的是否是墙的纵边，如果是，则突出显示纵边。
                         * 思考：首先平面地图中的每个字符'#'代表1块墙，我们可以从每块墙的4条纵边向主角发射测试射线，
                         * 然后分别于视野射线对比，若两种射线相仿（用点积的反余弦判断），则当前视野射线击中的墙面就可以看作墙的纵边。 */

                        std::vector<std::pair<float, float>> pv;  // 用于存储每块墙的4条测试射线长度以及与视野射线偏差弧度的容器。
                        // 用嵌套循环遍历一块墙的4条纵边相对于它自己的坐标位置。
                        for (int wx = 0; wx < 2; ++wx)
                            for (int wy = 0; wy < 2; ++wy) {
                                /* 计算当前纵边测试射线相对于主角在平面地图中的x、y坐标值。 */
                                float testRayX = viewRayX + wx - g_playerX;
                                float testRayY = viewRayY + wy - g_playerY;
                                // 计算当前纵边测试射线在平面地图中到主角的长度。
                                float testRayLength = sqrt(testRayX * testRayX + testRayY * testRayY);
                                /* 计算当前测试射线对应单位二维向量的x、y值。 */
                                float testRayNorVecX = testRayX / testRayLength;
                                float testRayNorVecY = testRayY / testRayLength;
                                // 计算两种射线的点积结果。
                                float dotRst = testRayNorVecX * viewRayNorVecX + testRayNorVecY * viewRayNorVecY;
                                // 用反余弦函数计算两种射线的夹角弧度。
                                float radDiff = acos(dotRst);
                                pv.emplace_back(std::make_pair(testRayLength, radDiff));  // 将所需数据存入对应容器。
                            }
                        /* 在pv容器中，以测试射线的长度进行升序排列，越靠前说明墙的纵边离主角越近。
                         * sort函数的第3个参数我们传入了一个lambda表达式，返回值规定了容器中任意相邻两个向量元素的比较规则。 */
                        std::sort(pv.begin(), pv.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                        float compVal = 0.01f;  // 用于比较两种射线夹角弧度的参考值。
                        // 若据主角最近的3条测试射线中，任意1条的对应结果小于参考值，则将视野射线标识为已击中墙的纵边。
                        // 最远的那条测试射线对应墙纵边在3D空间中是被遮挡的，因此不考虑。
                        if (pv[0].second < compVal || pv[1].second < compVal || pv[2].second < compVal) viewRayHitWallCor = true;
                    }
                }
            }

            /* 计算游戏屏幕中当前列墙面与天花板和地板接壤位置的行坐标。（主角与墙面在平面地图中间隔1个字符时，游戏屏幕的宽高即为两个接壤处的行坐标。 */
            int ceilingY = (float)SCR_H * 0.5f - (float)SCR_H / viewRayLength;
            int floorY   = SCR_H - ceilingY;

            wchar_t wallChar;  // 用于存储不同墙面的字符。

            if (viewRayHitWallCor) wallChar = 0x2591;  // 如果视野射线击中的墙纵边，将墙面字符设为'░'。
            else {
                /* 根据视野射线长度和主角视野深度计算选择不同墙面字符。 */
                if      (viewRayLength < g_playerViewDep * 0.3f) wallChar = 0x2588;  // '█'
                else if (viewRayLength < g_playerViewDep * 0.6f) wallChar = 0x2593;  // '▓'
                else if (viewRayLength < g_playerViewDep * 0.9f) wallChar = 0x2592;  // '▒'
                else if (viewRayLength < g_playerViewDep)        wallChar = 0x2591;  // '░'
                else                                             wallChar = ' ';
            }

            // 遍历屏幕中的每一行。
            for (int y = 0; y < SCR_H; ++y) {
                if      (y <  ceilingY)                chars[x + y * SCR_W] = ' ';  // 设置天花板字符为空格字符。
                else if (y >= ceilingY && y <= floorY) chars[x + y * SCR_W] = wallChar;  // 设置墙面字符为wallChar存储的字符。
                else                                   chars[x + y * SCR_W] = '-';       // 设置地板字符为'-'。
            }
        }

        // 判断主角是否到达了出口，是则结束游戏。
        if (g_map[(int)g_playerX + (int)g_playerY * MAP_W] == 'E') g_isGameRunning = false;

        /******************************************************************************************
         * 渲染画面。
         */

        // 在屏幕中显示平面地图。
        for (int x = 0; x < MAP_W; ++x)
            for (int y = 0; y < MAP_H; ++y)
                chars[x + y * SCR_W] = g_map[x + y * MAP_W];
        chars[(int)g_playerX + (int)g_playerY * SCR_W] = '@';  // 在平面地图中显示主角。

        // 将字符数组从指定显示位置开始拷贝入控制台屏幕缓冲区的连续单元中（将字符数组显示出来）。参数分别为：
        // 1、控制台屏幕缓冲区句柄（必须右GENERIC_WRITE访问权限）；2、一个指向将要写入控制台屏幕缓冲区的字符数组的指针；
        // 3、将要写入的字符数组的长度；4、坐标结构体变量（指定控制台屏幕缓冲区中首个显示单元的坐标位置）；
        // 5、一个指向用于接收所写字符数组长度的变量的指针。
        WriteConsoleOutputCharacterW(console, chars, SCR_W * SCR_H, { 0, 0 }, &bytesWritten);
    }

    return 0;
}
