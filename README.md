# TCC: Integração de ROS2 Foxy, Unreal Engine 5.3 e Georeferenciamento

**Autor:** Natan Ferreira Rosa de Jesus Vaz — Trabalho de Conclusão de Curso

**Universidade:** Universidade Federal de Itajubá

Este repositório apresenta o desenvolvimento de um Trabalho de Conclusão de Curso (TCC) que demonstra uma integração completa entre Unreal Engine 5.3, ROS2 Foxy, o plugin rclUE e o sistema de georreferenciamento Cesium for Unreal. O objetivo foi criar uma aplicação que recebe e publica mensagens ROS2 em tempo real dentro de uma cena 3D georreferenciada.

---

## Requisitos

- **Unreal Engine 5.3** (compilado para Linux)
- **ROS2 Foxy** instalado e configurado
- **Plugin rclUE** (copie para `Plugins/` se não estiver instalado globalmente)
- **Plugin Cesium for Unreal** (para georeferenciamento)
- **Linux** (testado no Ubuntu)

---

## Estrutura do Projeto

- `Source/` — Código C++ do projeto (MyActor, ROS2Connector, MyUserWidget, PlayerController)
- `Config/` — Configurações do projeto Unreal
- `Content/` — Assets essenciais para o funcionamento mínimo
- `Plugins/rclUE/` — Integração ROS2 ↔ Unreal
- `Plugins/Cesium/` — Georeferenciamento e conversão para coordenadas geodésicas
- `Campus_Itabira.uproject` — Arquivo do projeto Unreal

---

## Instalação e Execução

1. **Clone o repositório** e entre na pasta do projeto:
   ```bash
   git clone https://github.com/SeuUsuario/SeuRepo.git  
   cd SeuRepo
   ```
2. **Configure o ambiente ROS2**:
   ```bash
   source /opt/ros/foxy/setup.bash
   ```
3. **Abra o Unreal Editor pelo terminal**:
   ```bash
   /caminho/para/UnrealEngine/Engine/Binaries/Linux/UnrealEditor Campus_Itabira.uproject
   ```
4. **Compile o projeto** (se necessário):
   - Pelo menu do Editor ou via `make` se estiver configurado para build automático.
5. **Adicione os atores à cena**:
   - `MyActor` (subscriber ROS2 no tópico `/teste_unreal`)
   - `ROS2Connector` (inicializa o nó e gerencia publishers/subscribers)
   - `CesiumGeoreference` (objeto que faz a conversão de coordenadas)
6. **Publique e visualize mensagens ROS2**:
   ```bash
   ros2 topic pub /teste_unreal std_msgs/msg/String "{data: 'Hello Unreal'}"
   ```
   Verifique no Output Log do Editor a chegada da mensagem.
7. **Exiba latitude e longitude em HUD**:
   - O `PlayerController` instancia um `UserWidget` que atualiza dois `TextBlock` (Latitude, Longitude) via `ACesiumGeoreference` em `AMyActor::Tick()`.

---

## Cálculos Utilizados

- **Conversão de unidades**:
  ```cpp
  double X_m = GetActorLocation().X / 100.0;
  double Y_m = GetActorLocation().Y / 100.0;
  double Z_m = GetActorLocation().Z / 100.0;
  ```
- **Transformação ECEF ↔ Geodésico (WGS84)**:
  - Latitude: ϕ = arctan2(Z, √(X² + Y²))
  - Longitude: λ = arctan2(Y, X)
  - Altitude: diferença entre o raio ao ponto e o semi-eixo maior (a = 6378137 m)
- **Cálculo de latência ROS2**:
  Ao receber a mensagem em `MyActor`, registramos `rclcpp::Time::now()` e comparamos com o timestamp publicado para avaliar atrasos.

---

## Georeferenciamento

1. **Adição do objeto de georefência**:
   - Na cena, adicione o ator `CesiumGeoreference`, que gerencia a conversão de coordenadas.
2. **Implementação em C++ (`AMyActor::Tick`)**:
   ```cpp
   if (ACesiumGeoreference* Georef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld())) {
       double Latitude, Longitude, Height;
       Georef->TransformLongitudeLatitudeHeight(
           GetActorLocation(), Latitude, Longitude, Height);
       auto PC = GetWorld()->GetFirstPlayerController<ACampusPlayerController>();
       PC->GetHUDWidget()->SetLatLon(Latitude, Longitude);
   }
   ```

![HUD de Latitude e Longitude](Screenshot%20from%202025-04-18%2022-52-58.png)

---

## Observações

- Para detalhes do plugin rclUE: https://github.com/RobotecAI/rclUE
- Para documentação Cesium for Unreal: https://github.com/CesiumGS/cesium-unreal

---

## Contato

- **E‑mail:** natanvaz27@unifei.edu.br
- **LinkedIn:** https://www.linkedin.com/in/natao-vaz
