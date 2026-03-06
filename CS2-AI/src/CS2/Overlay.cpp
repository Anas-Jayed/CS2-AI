#include "CS2/Overlay.h"
#include <Windows.h>

Overlay::Overlay(GameInformationhandler* handler, QWidget* parent)
    : QWidget(parent), m_handler(handler)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    resize(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_timer->start(16);
}

bool Overlay::worldToScreen(const Vec3D<float>& worldPos, Vec2D<float>& screenPos, const ViewMatrix& vm) {
    float w = vm.matrix[12] * worldPos.x + vm.matrix[13] * worldPos.y + vm.matrix[14] * worldPos.z + vm.matrix[15];

    if (w < 0.01f) return false;

    float x = vm.matrix[0] * worldPos.x + vm.matrix[1] * worldPos.y + vm.matrix[2] * worldPos.z + vm.matrix[3];
    float y = vm.matrix[4] * worldPos.x + vm.matrix[5] * worldPos.y + vm.matrix[6] * worldPos.z + vm.matrix[7];

    float invW = 1.0f / w;
    float x_ndc = x * invW;
    float y_ndc = y * invW;

    float screenWidth = 1920.0f;
    float screenHeight = 1200.0f;

    screenPos.x = (screenWidth / 2.0f) + (x_ndc * screenWidth / 2.0f);
    screenPos.y = (screenHeight / 2.0f) - (y_ndc * screenHeight / 2.0f);

    return true;
}

void Overlay::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    if (!m_handler || !m_handler->esp_enabled) return;

    m_handler->update_game_information();

    ViewMatrix currentMatrix = m_handler->get_view_matrix();
    GameInformation game_info = m_handler->get_game_information();

    for (const auto& player : game_info.other_players) {

        if (player.health <= 0) continue;
        if (player.team == game_info.controlled_player.team) continue;

        Vec2D<float> headScreen;
        Vec2D<float> feetScreen;

        bool headInView = worldToScreen(player.head_position, headScreen, currentMatrix);
        bool feetInView = worldToScreen(player.position, feetScreen, currentMatrix);

        if (headInView && feetInView) {


            float height = feetScreen.y - headScreen.y;

            float boxHeight = height * 1.15f;
            float boxWidth = boxHeight / 2.0f;

            painter.setPen(QPen(Qt::red, 2));

            painter.drawRect(
                static_cast<int>(headScreen.x - (boxWidth / 2.0f)),
                static_cast<int>(headScreen.y - (height * 0.15f)),
                static_cast<int>(boxWidth),
                static_cast<int>(boxHeight)
            );

            painter.setPen(Qt::white);
            painter.drawText(
                static_cast<int>(headScreen.x + (boxWidth / 2.0f) + 2),
                static_cast<int>(headScreen.y),
                QString::number(player.health)
            );
        }
    }
}