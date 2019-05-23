// https://github.com/CedricGuillemet/Imogen
//
// The MIT License(MIT)
//
// Copyright(c) 2019 Cedric Guillemet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "Platform.h"
#include "NodeGraphControler.h"
#include "EvaluationStages.h"
#include "Library.h"
#include "EvaluationContext.h"
#include "Evaluators.h"
#include "UI.h"
#include "Utils.h"

void AddExtractedView(size_t nodeIndex);

NodeGraphControler::NodeGraphControler()
    : mbMouseDragging(false), mbUsingMouse(false), mEditingContext(mModel.mEvaluationStages, false, 1024, 1024)
{
}

void NodeGraphControler::Clear()
{
    mSelectedNodeIndex = -1;
    mBackgroundNode = -1;
    mModel.Clear();
    mEditingContext.Clear();
}

void NodeGraphControler::HandlePin(size_t nodeIndex, size_t parameterIndex)
{
    bool checked = mModel.IsParameterPinned(nodeIndex, parameterIndex);
    if (ImGui::Checkbox("", &checked))
    {
        mModel.BeginTransaction(true);
        mModel.SetParameterPin(nodeIndex, parameterIndex, checked);
        mModel.EndTransaction();
    }
}

bool NodeGraphControler::EditSingleParameter(unsigned int nodeIndex,
                                             unsigned int parameterIndex,
                                             void* paramBuffer,
                                             const MetaParameter& param)
{
    bool dirty = false;
    uint32_t parameterPair = (uint32_t(nodeIndex) << 16) + parameterIndex;
    ImGui::PushID(parameterPair * 4);
    HandlePin(nodeIndex, parameterIndex);
    ImGui::SameLine();
    switch (param.mType)
    {
        case Con_Float:
            dirty |= ImGui::InputFloat(param.mName.c_str(), (float*)paramBuffer);
            break;
        case Con_Float2:
            dirty |= ImGui::InputFloat2(param.mName.c_str(), (float*)paramBuffer);
            break;
        case Con_Float3:
            dirty |= ImGui::InputFloat3(param.mName.c_str(), (float*)paramBuffer);
            break;
        case Con_Float4:
            dirty |= ImGui::InputFloat4(param.mName.c_str(), (float*)paramBuffer);
            break;
        case Con_Color4:
            dirty |= ImGui::ColorPicker4(param.mName.c_str(), (float*)paramBuffer);
            break;
        case Con_Int:
            dirty |= ImGui::InputInt(param.mName.c_str(), (int*)paramBuffer);
            break;
        case Con_Int2:
            dirty |= ImGui::InputInt2(param.mName.c_str(), (int*)paramBuffer);
            break;
        case Con_Ramp:
        {
            RampEdit curveEditDelegate;
            curveEditDelegate.mPointCount = 0;
            for (int k = 0; k < 8; k++)
            {
                curveEditDelegate.mPts[k] = ImVec2(((float*)paramBuffer)[k * 2], ((float*)paramBuffer)[k * 2 + 1]);
                if (k && curveEditDelegate.mPts[k - 1].x > curveEditDelegate.mPts[k].x)
                    break;
                curveEditDelegate.mPointCount++;
            }
            float regionWidth = ImGui::GetWindowContentRegionWidth();
            if (ImCurveEdit::Edit(curveEditDelegate, ImVec2(regionWidth, regionWidth), 974))
            {
                for (size_t k = 0; k < curveEditDelegate.mPointCount; k++)
                {
                    ((float*)paramBuffer)[k * 2] = curveEditDelegate.mPts[k].x;
                    ((float*)paramBuffer)[k * 2 + 1] = curveEditDelegate.mPts[k].y;
                }
                ((float*)paramBuffer)[0] = 0.f;
                ((float*)paramBuffer)[(curveEditDelegate.mPointCount - 1) * 2] = 1.f;
                for (size_t k = curveEditDelegate.mPointCount; k < 8; k++)
                {
                    ((float*)paramBuffer)[k * 2] = -1.f;
                }
                dirty = true;
            }
        }
        break;
        case Con_Ramp4:
        {
            float regionWidth = ImGui::GetWindowContentRegionWidth();
            GradientEdit gradientDelegate;

            gradientDelegate.mPointCount = 0;

            for (int k = 0; k < 8; k++)
            {
                gradientDelegate.mPts[k] = ((ImVec4*)paramBuffer)[k];
                if (k && gradientDelegate.mPts[k - 1].w > gradientDelegate.mPts[k].w)
                    break;
                gradientDelegate.mPointCount++;
            }

            int colorIndex;
            dirty |= ImGradient::Edit(gradientDelegate, ImVec2(regionWidth, 22), colorIndex);
            if (colorIndex != -1)
            {
                dirty |= ImGui::ColorPicker3("", &gradientDelegate.mPts[colorIndex].x);
            }
            if (dirty)
            {
                for (size_t k = 0; k < gradientDelegate.mPointCount; k++)
                {
                    ((ImVec4*)paramBuffer)[k] = gradientDelegate.mPts[k];
                }
                ((ImVec4*)paramBuffer)[0].w = 0.f;
                ((ImVec4*)paramBuffer)[gradientDelegate.mPointCount - 1].w = 1.f;
                for (size_t k = gradientDelegate.mPointCount; k < 8; k++)
                {
                    ((ImVec4*)paramBuffer)[k].w = -1.f;
                }
            }
        }
        break;
        case Con_Angle:
            ((float*)paramBuffer)[0] = RadToDeg(((float*)paramBuffer)[0]);
            dirty |= ImGui::InputFloat(param.mName.c_str(), (float*)paramBuffer);
            ((float*)paramBuffer)[0] = DegToRad(((float*)paramBuffer)[0]);
            break;
        case Con_Angle2:
            ((float*)paramBuffer)[0] = RadToDeg(((float*)paramBuffer)[0]);
            ((float*)paramBuffer)[1] = RadToDeg(((float*)paramBuffer)[1]);
            dirty |= ImGui::InputFloat2(param.mName.c_str(), (float*)paramBuffer);
            ((float*)paramBuffer)[0] = DegToRad(((float*)paramBuffer)[0]);
            ((float*)paramBuffer)[1] = DegToRad(((float*)paramBuffer)[1]);
            break;
        case Con_Angle3:
            ((float*)paramBuffer)[0] = RadToDeg(((float*)paramBuffer)[0]);
            ((float*)paramBuffer)[1] = RadToDeg(((float*)paramBuffer)[1]);
            ((float*)paramBuffer)[2] = RadToDeg(((float*)paramBuffer)[2]);
            dirty |= ImGui::InputFloat3(param.mName.c_str(), (float*)paramBuffer);
            ((float*)paramBuffer)[0] = DegToRad(((float*)paramBuffer)[0]);
            ((float*)paramBuffer)[1] = DegToRad(((float*)paramBuffer)[1]);
            ((float*)paramBuffer)[2] = DegToRad(((float*)paramBuffer)[2]);
            break;
        case Con_Angle4:
            ((float*)paramBuffer)[0] = RadToDeg(((float*)paramBuffer)[0]);
            ((float*)paramBuffer)[1] = RadToDeg(((float*)paramBuffer)[1]);
            ((float*)paramBuffer)[2] = RadToDeg(((float*)paramBuffer)[2]);
            ((float*)paramBuffer)[3] = RadToDeg(((float*)paramBuffer)[3]);
            dirty |= ImGui::InputFloat4(param.mName.c_str(), (float*)paramBuffer);
            ((float*)paramBuffer)[0] = DegToRad(((float*)paramBuffer)[0]);
            ((float*)paramBuffer)[1] = DegToRad(((float*)paramBuffer)[1]);
            ((float*)paramBuffer)[2] = DegToRad(((float*)paramBuffer)[2]);
            ((float*)paramBuffer)[3] = DegToRad(((float*)paramBuffer)[3]);
            break;
        case Con_FilenameWrite:
        case Con_FilenameRead:
            ImGui::PushID(parameterPair * 4 + 1);
            dirty |= ImGui::InputText("", (char*)paramBuffer, 1024);
            ImGui::SameLine();
            if (ImGui::Button("..."))
            {
                #ifdef NFD_OpenDialog
                nfdchar_t* outPath = NULL;
                nfdresult_t result = (param.mType == Con_FilenameRead) ? NFD_OpenDialog(NULL, NULL, &outPath)
                                                                       : NFD_SaveDialog(NULL, NULL, &outPath);

                if (result == NFD_OKAY)
                {
                    strcpy((char*)paramBuffer, outPath);
                    free(outPath);
                    dirty = true;
                }
                #endif
            }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::Text(param.mName.c_str());
            break;
        case Con_Enum:
        {
            std::string cbString = param.mEnumList;
            for (auto& c : cbString)
            {
                if (c == '|')
                    c = '\0';
            }
            dirty |= ImGui::Combo(param.mName.c_str(), (int*)paramBuffer, cbString.c_str());
        }
        break;
        case Con_ForceEvaluate:
            if (ImGui::Button(param.mName.c_str()))
            {
                EvaluationInfo evaluationInfo;
                evaluationInfo.forcedDirty = 1;
                evaluationInfo.uiPass = 0;
                mEditingContext.RunSingle(nodeIndex, evaluationInfo);
            }
            break;
        case Con_Bool:
        {
            bool checked = (*(int*)paramBuffer) != 0;
            if (ImGui::Checkbox(param.mName.c_str(), &checked))
            {
                *(int*)paramBuffer = checked ? 1 : 0;
                dirty = true;
            }
        }
        break;
        case Con_Camera:
            if (ImGui::Button("Reset"))
            {
                Camera* cam = (Camera*)paramBuffer;
                cam->mPosition = Vec4(0.f, 0.f, 0.f);
                cam->mDirection = Vec4(0.f, 0.f, 1.f);
                cam->mUp = Vec4(0.f, 1.f, 0.f);
            }
            break;
        case Con_Multiplexer:
        {
            float displayWidth = ImGui::GetContentRegionAvail().x;
            static const float iconWidth = 50.f;
            unsigned int displayCount = std::max(int(floorf(displayWidth/iconWidth)), 1);
            unsigned int lineCount = int(ceilf(8.f/float(displayCount)));
            unsigned int defaultTextureId = gImageCache.GetTexture("Stock/thumbnail-icon.png");
            
            // get inputs
            std::vector<int> inputs;
            std::vector<int> inputIndex;
            int currentIndex = -1;
            const auto& stage = mModel.mEvaluationStages.mStages[nodeIndex];
            for (auto input : stage.mInput.mInputs)
            {
                currentIndex++;
                if (input == -1)
                    continue;
                inputs.push_back(input);
                inputIndex.push_back(currentIndex);
            }
            // draw buttons
            ImGui::BeginGroup();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1,1));
            auto buttonsToDraw = inputs.size();;
            int index = 0;
            int& value = *(int*)paramBuffer;
            for (unsigned int line = 0;line<lineCount;line++)
            {
                for (unsigned int disp = 0;disp<displayCount;disp++)
                {
                    if (!buttonsToDraw)
                    {
                        continue;
                    }
                    ImGui::PushID(index);
                    auto texture = GetNodeTexture(inputs[index]);
                    if (ImGui::ImageButton((ImTextureID)(int64_t)texture, ImVec2(iconWidth, iconWidth), ImVec2(0,0), ImVec2(1,1), -1, ImVec4(0,0,0,1)))
                    {
                        dirty = true;
                        value = inputIndex[index];
                    }

                    if (value == inputIndex[index])
                    {
                        ImRect rc = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        drawList->AddRect(rc.Min, rc.Max, 0xFF0000FF, 2.f, 15, 2.f);
                    }
                    ImGui::PopID();
                    if (disp != (displayCount-1) && buttonsToDraw)
                    {
                        ImGui::SameLine();
                    }
                    buttonsToDraw--;
                    index++;
                }
            }
            ImGui::PopStyleVar();
            ImGui::EndGroup();
        }
        break;
    }
    ImGui::PopID();
    return dirty;
}

void NodeGraphControler::PinnedEdit()
{
    int dirtyNode = -1;
    Parameters dirtyParameters;
    for (const auto pin : mModel.GetParameterPins())
    {
        if (!pin)
        {
            continue;
        }
        unsigned int nodeIndex = (pin >> 16) & 0xFFFF;
        unsigned int parameterIndex = pin & 0xFFFF;

        size_t nodeType = mModel.mEvaluationStages.mStages[nodeIndex].mType;
        const MetaNode& metaNode = gMetaNodes[nodeType];
        if (parameterIndex >= metaNode.mParams.size())
            continue;

        ImGui::PushID(171717 + pin);
        const MetaParameter& metaParam = metaNode.mParams[parameterIndex];
        auto parameters = mModel.GetParameters(nodeIndex);
        unsigned char* paramBuffer = parameters.data();
        paramBuffer += GetParameterOffset(uint32_t(nodeType), parameterIndex);
        if (EditSingleParameter(nodeIndex, parameterIndex, paramBuffer, metaParam))
		{
            dirtyNode = nodeIndex;
			dirtyParameters = parameters;
		}

        ImGui::PopID();
    }
    if (dirtyNode != -1)
    {
        mModel.SetParameters(dirtyNode, dirtyParameters);
    }
}

void NodeGraphControler::EditNodeParameters()
{
    size_t nodeIndex = mSelectedNodeIndex;

    const MetaNode* metaNodes = gMetaNodes.data();
    bool dirty = false;
    bool forceEval = false;
    bool samplerDirty = false;
    const auto& stage = mModel.mEvaluationStages.mStages[nodeIndex];
    const MetaNode& currentMeta = metaNodes[stage.mType];

    // edit samplers
    auto samplers = mModel.GetSamplers(nodeIndex);
    if (ImGui::CollapsingHeader("Samplers", 0))
    {
        for (size_t i = 0; i < samplers.size(); i++)
        {
            InputSampler& inputSampler = samplers[i];
            static const char* wrapModes = {"REPEAT\0EDGE\0BORDER\0MIRROR"};
            static const char* filterModes = {"LINEAR\0NEAREST"};
            ImGui::PushItemWidth(80);
            ImGui::PushID(int(99 + i));
            HandlePinIO(nodeIndex, i, false);
            ImGui::SameLine();
            ImGui::Text("Sampler %d", i);
            samplerDirty |= ImGui::Combo("U", (int*)&inputSampler.mWrapU, wrapModes);
            ImGui::SameLine();
            samplerDirty |= ImGui::Combo("V", (int*)&inputSampler.mWrapV, wrapModes);
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            samplerDirty |= ImGui::Combo("Min", (int*)&inputSampler.mFilterMin, filterModes);
            ImGui::SameLine();
            samplerDirty |= ImGui::Combo("Mag", (int*)&inputSampler.mFilterMag, filterModes);
            ImGui::PopItemWidth();
            ImGui::PopID();
            ImGui::PopItemWidth();
        }
        if (samplerDirty)
        {
            mModel.BeginTransaction(true);
            mModel.SetSamplers(nodeIndex, samplers);
            mModel.EndTransaction();
            mEditingContext.SetTargetDirty(nodeIndex, Dirty::Sampler);
        }
    }
    if (!ImGui::CollapsingHeader(currentMeta.mName.c_str(), 0, ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // edit parameters
    auto parameters = mModel.GetParameters(nodeIndex);
    auto paramBuffer = parameters.data();
    int i = 0;
    for (const MetaParameter& param : currentMeta.mParams)
    {
        ImGui::PushID(667889 + i);

        dirty |= EditSingleParameter((unsigned int)(nodeIndex), i, paramBuffer, param);

        ImGui::PopID();
        paramBuffer += GetParameterTypeSize(param.mType);
        i++;
    }

    if (dirty)
    {
        mModel.BeginTransaction(true);
        mModel.SetParameters(nodeIndex, parameters);
        mModel.EndTransaction();
    }
}

void NodeGraphControler::HandlePinIO(size_t nodeIndex, size_t slotIndex, bool forOutput)
{
    if (mModel.IsIOUsed(nodeIndex, int(slotIndex), forOutput))
    {
        return;
    }
    ImGui::PushID(int(nodeIndex * 256 + slotIndex * 2 + (forOutput ? 1 : 0)));
    bool pinned = mModel.IsIOPinned(nodeIndex, slotIndex, forOutput);
    if (ImGui::Checkbox("", &pinned))
    {
        mModel.BeginTransaction(true);
        mModel.SetIOPin(nodeIndex, slotIndex, forOutput, pinned);
        mModel.EndTransaction();
    }
    ImGui::PopID();
}

void NodeGraphControler::NodeEdit()
{
    ImGuiIO& io = ImGui::GetIO();

    if (mSelectedNodeIndex == -1)
    {
        auto& io = mModel.GetIOPins();
        for (size_t nodeIndex = 0; nodeIndex < io.size(); nodeIndex++)
        {
            if ((io[nodeIndex] & 1) == 0)
            {
                continue;
            }
            ImGui::PushID(int(1717171 + nodeIndex));
            uint32_t parameterPair = (uint32_t(nodeIndex) << 16) + 0xDEAD;
            HandlePinIO(nodeIndex, 0, true);
            ImGui::SameLine();
            Imogen::RenderPreviewNode(int(nodeIndex), *this);
            ImGui::PopID();
        }
        PinnedEdit();
    }
    else
    {
        if (ImGui::CollapsingHeader("Preview", 0, ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID(1717171);
            ImGui::BeginGroup();
            HandlePinIO(mSelectedNodeIndex, 0, true);
            unsigned int maxiMini = gImageCache.GetTexture("Stock/MaxiMini.png");
            bool selectedNodeAsBackground = mBackgroundNode == mSelectedNodeIndex;
            float ofs = selectedNodeAsBackground ? 0.5f : 0.f;
            if (ImGui::ImageButton(
                    (ImTextureID)(uint64_t)maxiMini, ImVec2(12, 13), ImVec2(0.f + ofs, 1.f), ImVec2(0.5f + ofs, 0.f)))
            {
                mBackgroundNode = selectedNodeAsBackground ? -1 : mSelectedNodeIndex;
            }
            ImGui::EndGroup();
            ImGui::SameLine();
            Imogen::RenderPreviewNode(mSelectedNodeIndex, *this);
            ImGui::PopID();
        }

        EditNodeParameters();
    }
    // apply dirty list
    const auto& dirtyList = mModel.GetDirtyList();
    for (const auto& dirtyItem : dirtyList)
    {
        mEditingContext.SetTargetDirty(dirtyItem.mNodeIndex, dirtyItem.mFlags);
    }
    mModel.ClearDirtyList();
}

void NodeGraphControler::SetKeyboardMouse(const UIInput& input, bool bValidInput)
{
    if (mSelectedNodeIndex == -1)
        return;

    if (!input.mLButDown)
        mbMouseDragging = false;

    if (!input.mLButDown && !input.mRButDown && mModel.InTransaction() && mbUsingMouse)
    {
        mModel.EndTransaction();
        mbUsingMouse = false;
    }

    const MetaNode* metaNodes = gMetaNodes.data();
    size_t res = 0;
    const MetaNode& metaNode = metaNodes[mModel.mEvaluationStages.mStages[mSelectedNodeIndex].mType];

    Parameters parameters = mModel.GetParameters(mSelectedNodeIndex);
    unsigned char* paramBuffer = parameters.data();
    bool parametersDirty = false;

    // camera handling
    for (auto& param : metaNode.mParams)
    {
        float* paramFlt = (float*)paramBuffer;
        if (param.mType == Con_Camera)
        {
            Camera* cam = (Camera*)paramBuffer;
            if (fabsf(input.mWheel)>0.f)
            {
               cam->mPosition += cam->mDirection * input.mWheel;
                parametersDirty = true;
            }
            Vec4 right = Cross(cam->mUp, cam->mDirection);
            right.y = 0.f; // keep head up
            right.Normalize();
            auto& io = ImGui::GetIO();
            if (io.KeyAlt)
            {
                if (io.MouseDown[2])
                {
                    cam->mPosition += (right * io.MouseDelta.x + cam->mUp * io.MouseDelta.y) * 0.01f;
                    parametersDirty = true;
                }
                if (io.MouseDown[1])
                {
                    cam->mPosition += (cam->mDirection * io.MouseDelta.y) * 0.01f;
                    parametersDirty = true;
                }
                if (io.MouseDown[0])
                {
                    Mat4x4 tr, rtUp, rtRight, trp;
                    tr.Translation(-(cam->mPosition));
                    rtRight.RotationAxis(right, io.MouseDelta.y * 0.01f);
                    rtUp.RotationAxis(cam->mUp, -io.MouseDelta.x * 0.01f);
                    trp.Translation((cam->mPosition));
                    Mat4x4 res = tr * rtRight * rtUp * trp;
                    cam->mPosition.TransformPoint(res);
                    cam->mDirection.TransformVector(res);
                    cam->mUp.Cross(cam->mDirection, right);
                    cam->mUp.Normalize();
                    parametersDirty = true;
                }
            }
        }
        paramBuffer += GetParameterTypeSize(param.mType);
    }

    //
    paramBuffer = parameters.data();
    if (input.mLButDown)
    {
        for (auto& param : metaNode.mParams)
        {
            float* paramFlt = (float*)paramBuffer;
            if (param.mbQuadSelect && param.mType == Con_Float4)
            {
                parametersDirty = true;
                if (!mbMouseDragging)
                {
                    paramFlt[2] = paramFlt[0] = input.mRx;
                    paramFlt[3] = paramFlt[1] = 1.f - input.mRy;
                    mbMouseDragging = true;
                }
                else
                {
                    paramFlt[2] = input.mRx;
                    paramFlt[3] = 1.f - input.mRy;
                }
                continue;
            }

            if (param.mRangeMinX != 0.f || param.mRangeMaxX != 0.f)
            {
                parametersDirty = true;
                if (param.mbRelative)
                {
                    paramFlt[0] += (param.mRangeMaxX - param.mRangeMinX) * input.mDx;
                    if (param.mbLoop)
                        paramFlt[0] = fmodf(paramFlt[0], fabsf(param.mRangeMaxX - param.mRangeMinX)) +
                                      min(param.mRangeMinX, param.mRangeMaxX);
                }
                else
                {
                    paramFlt[0] = ImLerp(param.mRangeMinX, param.mRangeMaxX, input.mRx);
                }
            }
            if (param.mRangeMinY != 0.f || param.mRangeMaxY != 0.f)
            {
                parametersDirty = true;
                if (param.mbRelative)
                {
                    paramFlt[1] += (param.mRangeMaxY - param.mRangeMinY) * input.mDy;
                    if (param.mbLoop)
                        paramFlt[1] = fmodf(paramFlt[1], fabsf(param.mRangeMaxY - param.mRangeMinY)) +
                                      min(param.mRangeMinY, param.mRangeMaxY);
                }
                else
                {
                    paramFlt[1] = ImLerp(param.mRangeMinY, param.mRangeMaxY, input.mRx);
                }
            }
            paramBuffer += GetParameterTypeSize(param.mType);
        }
    }
    if (metaNode.mbHasUI || parametersDirty)
    {
        mModel.SetKeyboardMouse(mSelectedNodeIndex, input);

        if ((input.mLButDown || input.mRButDown) && !mModel.InTransaction() && parametersDirty && bValidInput)
        {
            mModel.BeginTransaction(true);
            mbUsingMouse = true;
        }
        if (parametersDirty && mModel.InTransaction() && bValidInput)
		{
            mModel.SetParameters(mSelectedNodeIndex, parameters);
		}
        mEditingContext.SetTargetDirty(mSelectedNodeIndex, Dirty::Mouse);
    }
}

void NodeGraphControler::ContextMenu(ImVec2 scenePos, int nodeHovered)
{
    ImGuiIO& io = ImGui::GetIO();
    size_t metaNodeCount = gMetaNodes.size();
    const MetaNode* metaNodes = gMetaNodes.data();
    const auto& nodes = mModel.GetNodes();

    bool copySelection = false;
    bool deleteSelection = false;
    bool pasteSelection = false;

    // Draw context menu
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    if (ImGui::BeginPopup("context_menu"))
    {
        auto* node = nodeHovered != -1 ? &nodes[nodeHovered] : NULL;
        if (node)
        {
            ImGui::Text(metaNodes[node->mType].mName.c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Extract view", NULL, false))
            {
                AddExtractedView(nodeHovered);
            }
        }
        else
        {
            auto AddNode = [&](int nodeType) {
                mModel.BeginTransaction(true);
                mModel.AddNode(nodeType, scenePos);
                mModel.EndTransaction();
            };

            static char inputText[64] = {0};
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            ImGui::InputText("", inputText, sizeof(inputText));
            {
                if (strlen(inputText))
                {
                    for (int i = 0; i < metaNodeCount; i++)
                    {
                        const char* nodeName = metaNodes[i].mName.c_str();
                        bool displayNode =
                            !strlen(inputText) ||
                            ImStristr(nodeName, nodeName + strlen(nodeName), inputText, inputText + strlen(inputText));
                        if (displayNode && ImGui::MenuItem(nodeName, NULL, false))
                        {
                            AddNode(i);
                        }
                    }
                }
                else
                {
                    for (int i = 0; i < metaNodeCount; i++)
                    {
                        const char* nodeName = metaNodes[i].mName.c_str();
                        if (metaNodes[i].mCategory == -1 && ImGui::MenuItem(nodeName, NULL, false))
                        {
                            AddNode(i);
                        }
                    }

					const auto& categories = MetaNode::mCategories;
                    for (unsigned int iCateg = 0; iCateg < categories.size(); iCateg++)
                    {
                        if (ImGui::BeginMenu(categories[iCateg].c_str()))
                        {
                            for (int i = 0; i < metaNodeCount; i++)
                            {
                                const char* nodeName = metaNodes[i].mName.c_str();
                                if (metaNodes[i].mCategory == iCateg && ImGui::MenuItem(nodeName, NULL, false))
                                {
                                    AddNode(i);
                                }
                            }
                            ImGui::EndMenu();
                        }
                    }
                }
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Add rug", NULL, false))
        {
            GraphModel::Rug rug = {
                scenePos, ImVec2(400, 200), 0xFFA0A0A0, "Description\nEdit me with a double click."};
            mModel.BeginTransaction(true);
            mModel.AddRug(rug);
            mModel.EndTransaction();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Del", false))
        {
            deleteSelection = true;
        }
        if (ImGui::MenuItem("Copy", "CTRL+C"))
        {
            copySelection = true;
        }
        if (ImGui::MenuItem("Paste", "CTRL+V", false, !mModel.IsClipboardEmpty()))
        {
            pasteSelection = true;
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    if (copySelection || (ImGui::IsWindowFocused() && io.KeyCtrl && ImGui::IsKeyPressedMap(ImGuiKey_C)))
    {
        mModel.CopySelectedNodes();
    }

    if (deleteSelection || (ImGui::IsWindowFocused() && ImGui::IsKeyPressedMap(ImGuiKey_Delete)))
    {
        mModel.CutSelectedNodes();
    }

    if (pasteSelection || (ImGui::IsWindowFocused() && io.KeyCtrl && ImGui::IsKeyPressedMap(ImGuiKey_V)))
    {
        mModel.PasteNodes(scenePos);
    }
}
bool NodeGraphControler::NodeIs2D(size_t nodeIndex) const
{
    auto target = mEditingContext.GetRenderTarget(nodeIndex);
    if (target)
        return target->mImage->mNumFaces == 1;
    return false;
}

bool NodeGraphControler::NodeIsCompute(size_t nodeIndex) const
{
    return (gEvaluators.GetMask(mModel.mEvaluationStages.mStages[nodeIndex].mType) & EvaluationGLSLCompute) != 0;
}

bool NodeGraphControler::NodeIsCubemap(size_t nodeIndex) const
{
    auto target = mEditingContext.GetRenderTarget(nodeIndex);
    if (target)
        return target->mImage->mNumFaces == 6;
    return false;
}

ImVec2 NodeGraphControler::GetEvaluationSize(size_t nodeIndex) const
{
    int imageWidth(1), imageHeight(1);
    EvaluationAPI::GetEvaluationSize(&mEditingContext, int(nodeIndex), &imageWidth, &imageHeight);
    return ImVec2(float(imageWidth), float(imageHeight));
}

void NodeGraphControler::DrawNodeImage(ImDrawList* drawList,
                                       const ImRect& rc,
                                       const ImVec2 marge,
                                       const size_t nodeIndex)
{
    if (NodeIsProcesing(nodeIndex) == 1)
    {
        AddUICustomDraw(drawList, rc, DrawUICallbacks::DrawUIProgress, nodeIndex, &mEditingContext);
    }
    else if (NodeIsCubemap(nodeIndex))
    {
        AddUICustomDraw(drawList, rc, DrawUICallbacks::DrawUICubemap, nodeIndex, &mEditingContext);
    }
    else if (NodeIsCompute(nodeIndex))
    {
    }
    else
    {
        drawList->AddImage((ImTextureID)(int64_t)(GetNodeTexture(size_t(nodeIndex))),
                           rc.Min + marge,
                           rc.Max - marge,
                           ImVec2(0, 1),
                           ImVec2(1, 0));
    }
}

bool NodeGraphControler::RenderBackground()
{
    if (mBackgroundNode != -1)
    {
        Imogen::RenderPreviewNode(mBackgroundNode, *this, true);
        return true;
    }
    return false;
}
