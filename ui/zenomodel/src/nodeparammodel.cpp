#include "nodeparammodel.h"
#include "vparamitem.h"
#include "modelrole.h"
#include "globalcontrolmgr.h"
#include "uihelper.h"
#include "variantptr.h"
#include "globalcontrolmgr.h"
#include "dictkeymodel.h"
#include "iotags.h"


NodeParamModel::NodeParamModel(const QPersistentModelIndex& subgIdx, const QModelIndex& nodeIdx, IGraphsModel* pModel, bool bTempModel, QObject* parent)
    : ViewParamModel(true, nodeIdx, pModel, parent)
    , m_inputs(nullptr)
    , m_params(nullptr)
    , m_outputs(nullptr)
    , m_pGraphsModel(pModel)
    , m_subgIdx(subgIdx)
    , m_bTempModel(bTempModel)
{
    initUI();
    connect(this, &NodeParamModel::modelAboutToBeReset, this, &NodeParamModel::onModelAboutToBeReset);
    connect(this, &NodeParamModel::rowsAboutToBeRemoved, this, &NodeParamModel::onRowsAboutToBeRemoved);
    if (!m_bTempModel && m_pGraphsModel->IsSubGraphNode(m_nodeIdx))
    {
        GlobalControlMgr &mgr = GlobalControlMgr::instance();
        connect(this, &NodeParamModel::rowsInserted, &mgr, &GlobalControlMgr::onCoreParamsInserted);
        connect(this, &NodeParamModel::rowsAboutToBeRemoved, &mgr, &GlobalControlMgr::onCoreParamsAboutToBeRemoved);
    }
}

NodeParamModel::~NodeParamModel()
{
}

void NodeParamModel::onModelAboutToBeReset()
{
    clearParams();
}

void NodeParamModel::clearParams()
{
    while (m_inputs->rowCount() > 0)
    {
        m_inputs->removeRows(0, 1);
    }
    while (m_params->rowCount() > 0)
    {
        m_params->removeRows(0, 1);
    }
    while (m_outputs->rowCount() > 0)
    {
        m_outputs->removeRows(0, 1);
    }
}

void NodeParamModel::initUI()
{
    /* structure:
      invisibleroot
            |-- Inputs (Group)
                -- input param1 (Item)
                -- input param2
                ...

            |-- Params (Group)
                -- param1 (Item)
                -- param2 (Item)

            |- Outputs (Group)
                - output param1 (Item)
                - output param2 (Item)
    */
    m_inputs = new VParamItem(VPARAM_GROUP, iotags::params::node_inputs);
    m_params = new VParamItem(VPARAM_GROUP, iotags::params::node_params);
    m_outputs = new VParamItem(VPARAM_GROUP, iotags::params::node_outputs);
    appendRow(m_inputs);
    appendRow(m_params);
    appendRow(m_outputs);
}

bool NodeParamModel::getInputSockets(INPUT_SOCKETS& inputs)
{
    for (int r = 0; r < m_inputs->rowCount(); r++)
    {
        VParamItem* param = static_cast<VParamItem*>(m_inputs->child(r));
        const QString& name = param->m_name;

        INPUT_SOCKET inSocket;
        inSocket.info.defaultValue = param->m_value;
        inSocket.info.nodeid = m_nodeIdx.data(ROLE_OBJID).toString();
        inSocket.info.name = param->m_name;
        inSocket.info.type = param->m_type;
        inSocket.info.sockProp = param->m_sockProp;
        inSocket.info.links = exportLinks(param->m_links);
        inSocket.info.control = param->m_ctrl;

        //todo: dict key model as children of this item?
        inputs.insert(name, inSocket);
    }
    return true;
}

bool NodeParamModel::getOutputSockets(OUTPUT_SOCKETS& outputs)
{
    for (int r = 0; r < m_outputs->rowCount(); r++)
    {
        VParamItem* param = static_cast<VParamItem*>(m_outputs->child(r));
        const QString& name = param->m_name;

        OUTPUT_SOCKET outSocket;
        outSocket.info.defaultValue = param->m_value;
        outSocket.info.nodeid = m_nodeIdx.data(ROLE_OBJID).toString();
        outSocket.info.name = name;
        outSocket.info.type = param->m_type;
        outSocket.info.sockProp = param->m_sockProp;
        outSocket.info.links = exportLinks(param->m_links);

        outputs.insert(name, outSocket);
    }
    return true;
}

bool NodeParamModel::getParams(PARAMS_INFO &params)
{
    for (int r = 0; r < m_params->rowCount(); r++)
    {
        VParamItem* param = static_cast<VParamItem*>(m_params->child(r));
        const QString& name = param->m_name;

        PARAM_INFO paramInfo;
        paramInfo.bEnableConnect = false;
        paramInfo.value = param->m_value;
        paramInfo.typeDesc = param->m_type;
        paramInfo.name = name;
        params.insert(name, paramInfo);
    }
    return true;
}

VParamItem* NodeParamModel::getInputs() const
{
    return m_inputs;
}

VParamItem* NodeParamModel::getParams() const
{
    return m_params;
}

VParamItem* NodeParamModel::getOutputs() const
{
    return m_outputs;
}

QModelIndexList NodeParamModel::getInputIndice() const
{
    QModelIndexList lst;
    for (int i = 0; i < m_inputs->rowCount(); i++) {
        lst.append(m_inputs->child(i)->index());
    }
    return lst;
}

QModelIndexList NodeParamModel::getParamIndice() const
{
    QModelIndexList lst;
    for (int i = 0; i < m_params->rowCount(); i++) {
        lst.append(m_params->child(i)->index());
    }
    return lst;
}

QModelIndexList NodeParamModel::getOutputIndice() const
{
    QModelIndexList lst;
    for (int i = 0; i < m_outputs->rowCount(); i++) {
        lst.append(m_outputs->child(i)->index());
    }
    return lst;
}

void NodeParamModel::setInputSockets(const INPUT_SOCKETS& inputs)
{
    for (INPUT_SOCKET inSocket : inputs)
    {
        VParamItem* pItem = new VParamItem(VPARAM_PARAM, inSocket.info.name, false);
        pItem->m_name = inSocket.info.name;
        pItem->m_value = inSocket.info.defaultValue;
        pItem->m_type = inSocket.info.type;
        pItem->m_sockProp = (SOCKET_PROPERTY)inSocket.info.sockProp;

        // init control
        const QString &nodeCls = m_nodeIdx.data(ROLE_OBJNAME).toString();
        CONTROL_INFO infos =
            GlobalControlMgr::instance().controlInfo(nodeCls, PARAM_PARAM, pItem->m_name, pItem->m_type);

        QVariant ctrlProps;
        if (inSocket.info.control != CONTROL_NONE)
            pItem->m_ctrl = inSocket.info.control;
        else
            pItem->m_ctrl = infos.control;

        if (inSocket.info.ctrlProps == QVariant())
            ctrlProps = infos.controlProps;
        else
            ctrlProps = inSocket.info.ctrlProps;

        pItem->setData(ctrlProps, ROLE_VPARAM_CTRL_PROPERTIES);

        m_inputs->appendRow(pItem);

        //if current item is a dict socket, init dict sockets after new item insered, and then get the valid index to init dict model.
        initDictSocket(pItem);
    }
}

void NodeParamModel::setParams(const PARAMS_INFO& params)
{
    for (PARAM_INFO paramInfo : params)
    {
        VParamItem* pItem = new VParamItem(VPARAM_PARAM, paramInfo.name, false);
        pItem->m_name = paramInfo.name;
        pItem->m_value = paramInfo.value;
        pItem->m_type = paramInfo.typeDesc;
        pItem->m_sockProp = SOCKPROP_UNKNOWN;

        const QString& nodeCls = m_nodeIdx.data(ROLE_OBJNAME).toString();
        CONTROL_INFO infos =
            GlobalControlMgr::instance().controlInfo(nodeCls, PARAM_PARAM, pItem->m_name, pItem->m_type);

        QVariant ctrlProps;
        if (paramInfo.control != CONTROL_NONE)
            pItem->m_ctrl = paramInfo.control;
        else
            pItem->m_ctrl = infos.control;

        if (paramInfo.controlProps == QVariant())
            ctrlProps = infos.controlProps;
        else
            ctrlProps = paramInfo.controlProps;

        pItem->setData(ctrlProps, ROLE_VPARAM_CTRL_PROPERTIES);

        m_params->appendRow(pItem);
    }
}

void NodeParamModel::setOutputSockets(const OUTPUT_SOCKETS& outputs)
{
    for (OUTPUT_SOCKET outSocket : outputs)
    {
        VParamItem *pItem = new VParamItem(VPARAM_PARAM, outSocket.info.name, false);
        pItem->m_name = outSocket.info.name;
        pItem->m_value = outSocket.info.defaultValue;
        pItem->m_type = outSocket.info.type;
        pItem->m_sockProp = (SOCKET_PROPERTY)outSocket.info.sockProp;
        m_outputs->appendRow(pItem);
        initDictSocket(pItem);
    }
}

QList<EdgeInfo> NodeParamModel::exportLinks(const PARAM_LINKS& links)
{
    QList<EdgeInfo> linkInfos;
    for (auto linkIdx : links)
    {
        EdgeInfo link = exportLink(linkIdx);
        linkInfos.append(link);
    }
    return linkInfos;
}

EdgeInfo NodeParamModel::exportLink(const QModelIndex& linkIdx)
{
    EdgeInfo link;

    QModelIndex outSock = linkIdx.data(ROLE_OUTSOCK_IDX).toModelIndex();
    QModelIndex inSock = linkIdx.data(ROLE_INSOCK_IDX).toModelIndex();
    ZASSERT_EXIT(outSock.isValid() && inSock.isValid(), link);

    link.outSockPath = outSock.data(ROLE_OBJPATH).toString();
    link.inSockPath = inSock.data(ROLE_OBJPATH).toString();
    return link;
}

void NodeParamModel::removeParam(PARAM_CLASS cls, const QString& name)
{
    if (PARAM_INPUT == cls)
    {
        for (int i = 0; i < m_inputs->rowCount(); i++)
        {
            VParamItem* pChild = static_cast<VParamItem*>(m_inputs->child(i));
            if (pChild->m_name == name)
            {
                m_inputs->removeRow(i);
                return;
            }
        }
    }
    if (PARAM_PARAM == cls)
    {
        for (int i = 0; i < m_params->rowCount(); i++)
        {
            VParamItem* pChild = static_cast<VParamItem*>(m_params->child(i));
            if (pChild->m_name == name)
            {
                m_params->removeRow(i);
                return;
            }
        }
    }
    if (PARAM_OUTPUT == cls)
    {
        for (int i = 0; i < m_outputs->rowCount(); i++)
        {
            VParamItem* pChild = static_cast<VParamItem*>(m_outputs->child(i));
            if (pChild->m_name == name)
            {
                m_outputs->removeRow(i);
                return;
            }
        }
    }
}

void NodeParamModel::setAddParam(
                PARAM_CLASS cls,
                const QString& name,
                const QString& type,
                const QVariant& deflValue,
                PARAM_CONTROL ctrl,
                QVariant ctrlProps,
                SOCKET_PROPERTY prop)
{
    VParamItem *pItem = nullptr;
    const QString& nodeCls = m_nodeIdx.data(ROLE_OBJNAME).toString();

    CONTROL_INFO infos = GlobalControlMgr::instance().controlInfo(nodeCls, cls, name, type);
    if (ctrl == CONTROL_NONE)
    {
        ctrl = infos.control;
    }
    if (ctrlProps == QVariant())
    {
        ctrlProps = infos.controlProps;
    }

    if (PARAM_INPUT == cls)
    {
        if (!(pItem = m_inputs->getItem(name)))
        {
            pItem = new VParamItem(VPARAM_PARAM, name);
            m_inputs->appendRow(pItem);
            initDictSocket(pItem);
        }
        ZASSERT_EXIT(pItem);
        pItem->setData(ctrlProps, ROLE_VPARAM_CTRL_PROPERTIES);
        pItem->m_name = name;
        pItem->m_value = deflValue;
        pItem->m_type = type;
        pItem->m_sockProp = prop;
        pItem->m_ctrl = ctrl;
    }
    else if (PARAM_PARAM == cls)
    {
        if (!(pItem = m_params->getItem(name)))
        {
            pItem = new VParamItem(VPARAM_PARAM, name);
            m_params->appendRow(pItem);
        }
        ZASSERT_EXIT(pItem);
        pItem->m_name = name;
        pItem->m_value = deflValue;
        pItem->m_type = type;
        pItem->m_sockProp = prop;
        pItem->m_ctrl = ctrl;
        pItem->setData(ctrlProps, ROLE_VPARAM_CTRL_PROPERTIES);
    }
    else if (PARAM_OUTPUT == cls)
    {
        if (!(pItem = m_outputs->getItem(name)))
        {
            pItem = new VParamItem(VPARAM_PARAM, name);
            m_outputs->appendRow(pItem);
            initDictSocket(pItem);
        }
        ZASSERT_EXIT(pItem);
        pItem->m_name = name;
        pItem->m_value = deflValue;
        pItem->m_sockProp = prop;
        pItem->m_type = type;
        pItem->m_ctrl = ctrl;
        pItem->setData(ctrlProps, ROLE_VPARAM_CTRL_PROPERTIES);
    }
}

QVariant NodeParamModel::getValue(PARAM_CLASS cls, const QString& name) const
{
    VParamItem *pItem = nullptr;
    if (PARAM_INPUT == cls)
    {
        if (!(pItem = m_inputs->getItem(name)))
        {
            return QVariant();
        }
    }
    else if (PARAM_PARAM == cls)
    {
        if (!(pItem = m_params->getItem(name)))
        {
            return QVariant();
        }
    }
    else if (PARAM_OUTPUT == cls)
    {
        if (!(pItem = m_outputs->getItem(name)))
        {
            return QVariant();
        }
    }
    else
    {
        return QVariant();
    }
    return pItem->m_value;
}

QModelIndex NodeParamModel::getParam(PARAM_CLASS cls, const QString& name) const
{
    if (PARAM_INPUT == cls)
    {
        if (VParamItem* pItem = m_inputs->getItem(name))
        {
            return pItem->index();
        }
    }
    else if (PARAM_PARAM == cls)
    {
        if (VParamItem* pItem = m_params->getItem(name))
        {
            return pItem->index();
        }
    }
    else if (PARAM_OUTPUT == cls)
    {
        if (VParamItem* pItem = m_outputs->getItem(name))
        {
            return pItem->index();
        }
    }
    return QModelIndex();
}

QVariant NodeParamModel::data(const QModelIndex& index, int role) const
{
    VParamItem* pItem = static_cast<VParamItem*>(itemFromIndex(index));
    if (!pItem)
        return QVariant();

    switch (role)
    {
    case ROLE_PARAM_CLASS:
    {
        if (pItem->vType != VPARAM_PARAM)
            return QVariant();
        VParamItem* parentItem = static_cast<VParamItem*>(pItem->parent());
        if (iotags::params::node_inputs == parentItem->m_name)
            return PARAM_INPUT;
        else if (iotags::params::node_outputs == parentItem->m_name)
            return PARAM_OUTPUT;
        else if (iotags::params::node_params == parentItem->m_name)
            return PARAM_PARAM;
        return PARAM_UNKNOWN;
    }
    case ROLE_VPARAM_LINK_MODEL:
    {
        if (pItem->m_customData.find(role) != pItem->m_customData.end())
        {
            return pItem->m_customData[role];
        }
        break;
    }
    default:
        return ViewParamModel::data(index, role);
    }

}

QModelIndex NodeParamModel::indexFromPath(const QString& path)
{
    QStringList lst = path.split("/", Qt::SkipEmptyParts);
    if (lst.size() < 2)
        return QModelIndex();

    const QString& group = lst[0];
    const QString& name = lst[1];
    QString subkey = lst.size() > 2 ? lst[2] : "";

    if (group == iotags::params::node_inputs)
    {
        if (VParamItem* pItem = m_inputs->getItem(name))
        {
            if (!subkey.isEmpty())
            {
                if (pItem->m_customData.find(ROLE_VPARAM_LINK_MODEL) != pItem->m_customData.end())
                {
                    DictKeyModel* keyModel = QVariantPtr<DictKeyModel>::asPtr(pItem->m_customData[ROLE_VPARAM_LINK_MODEL]);
                    ZASSERT_EXIT(keyModel, QModelIndex());
                    return keyModel->index(subkey);
                }
            }
            return pItem->index();
        }
    }
    else if (group == iotags::params::node_params)
    {
        if (VParamItem* pItem = m_params->getItem(name))
        {
            return pItem->index();
        }
    }
    else if (group == iotags::params::node_outputs)
    {
        if (VParamItem* pItem = m_outputs->getItem(name))
        {
            if (!subkey.isEmpty())
            {
                if (pItem->m_customData.find(ROLE_VPARAM_LINK_MODEL) != pItem->m_customData.end())
                {
                    DictKeyModel* keyModel = QVariantPtr<DictKeyModel>::asPtr(pItem->m_customData[ROLE_VPARAM_LINK_MODEL]);
                    ZASSERT_EXIT(keyModel, QModelIndex());
                    return keyModel->index(subkey);
                }
            }
            return pItem->index();
        }
    }
    return QModelIndex();
}

QStringList NodeParamModel::sockNames(PARAM_CLASS cls) const
{
    QStringList names;
    if (cls == PARAM_INPUT)
    {
        for (int r = 0; r < m_inputs->rowCount(); r++)
        {
            VParamItem* pItem = static_cast<VParamItem*>(m_inputs->child(r));
            names.append(pItem->m_name);
        }
    }
    else if (cls == PARAM_PARAM)
    {
        for (int r = 0; r < m_params->rowCount(); r++)
        {
            VParamItem* pItem = static_cast<VParamItem*>(m_params->child(r));
            names.append(pItem->m_name);
        }
    }
    else if (cls == PARAM_OUTPUT)
    {
        for (int r = 0; r < m_outputs->rowCount(); r++)
        {
            VParamItem* pItem = static_cast<VParamItem*>(m_outputs->child(r));
            names.append(pItem->m_name);
        }
    }
    return names;
}

void NodeParamModel::clone(ViewParamModel* pModel)
{
    //only add params.
    NodeParamModel* pOtherModel = qobject_cast<NodeParamModel*>(pModel);
    for (int i = 0; i < pOtherModel->m_inputs->rowCount(); i++)
    {
        QStandardItem* item = pOtherModel->m_inputs->child(i)->clone();
        m_inputs->appendRow(item);
    }
    for (int i = 0; i < pOtherModel->m_params->rowCount(); i++)
    {
        QStandardItem* item = pOtherModel->m_params->child(i)->clone();
        m_params->appendRow(item);
    }
    for (int i = 0; i < pOtherModel->m_outputs->rowCount(); i++)
    {
        QStandardItem* item = pOtherModel->m_outputs->child(i)->clone();
        m_outputs->appendRow(item);
    }

    m_inputs->m_uuid = pOtherModel->m_inputs->m_uuid;
    m_params->m_uuid = pOtherModel->m_params->m_uuid;
    m_outputs->m_uuid = pOtherModel->m_outputs->m_uuid;
}

bool NodeParamModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    switch (role)
    {
        case ROLE_PARAM_NAME:
        {
            VParamItem* pItem = static_cast<VParamItem*>(itemFromIndex(index));
            const QString& oldName = pItem->m_name;
            const QString& newName = value.toString();
            if (oldName == newName)
                return false;

            pItem->setData(newName, role);
            if (!m_bTempModel && m_pGraphsModel->IsSubGraphNode(m_nodeIdx) && pItem->vType == VPARAM_PARAM)
            {
                VParamItem* parentItem = static_cast<VParamItem*>(pItem->parent());
                PARAM_CLASS cls = PARAM_UNKNOWN;
                if (parentItem->m_name == iotags::params::node_inputs)
                    cls = PARAM_INPUT;
                else if (parentItem->m_name == iotags::params::node_params)
                    cls = PARAM_PARAM;
                else if (parentItem->m_name == iotags::params::node_outputs)
                    cls = PARAM_OUTPUT;

                const QString& nodeCls = m_nodeIdx.data(ROLE_OBJNAME).toString();
                GlobalControlMgr::instance().onParamRename(nodeCls, cls, oldName, newName);
            }
            break;
        }
        case ROLE_PARAM_TYPE:
        {
            VParamItem* pItem = static_cast<VParamItem*>(itemFromIndex(index));
            if (pItem->m_type == value.toString())
                return false;

            pItem->setData(value, role);
            break;
        }
        case ROLE_PARAM_VALUE:
        {
            VParamItem* pItem = static_cast<VParamItem*>(itemFromIndex(index));
            QVariant oldValue = pItem->m_value;
            if (oldValue == value)
                return false;

            pItem->setData(value, role);
            onSubIOEdited(oldValue, pItem);
            break;
        }
        case ROLE_ADDLINK:
        case ROLE_REMOVELINK:
        {
            VParamItem* pItem = static_cast<VParamItem*>(itemFromIndex(index));
            if (pItem->vType != VPARAM_PARAM)
                return false;

            pItem->setData(value, role);

            if (role == ROLE_ADDLINK)
            {
                onLinkAdded(pItem);
            }
            break;
        }
    default:
        return ViewParamModel::setData(index, value, role);
    }
}

void NodeParamModel::clearLinks(VParamItem* pItem)
{
    for (const QPersistentModelIndex& linkIdx : pItem->m_links)
    {
        m_pGraphsModel->removeLink(linkIdx, true);
    }
    pItem->m_links.clear();
}

void NodeParamModel::initDictSocket(VParamItem* pItem)
{
    if (!pItem || pItem->vType != VPARAM_PARAM)
        return;

    const QString& nodeCls = m_nodeIdx.data(ROLE_OBJNAME).toString();
    NODE_DESC desc;
    m_model->getDescriptor(nodeCls, desc);

    if (pItem->m_type == "dict" || pItem->m_type == "DictObject" || pItem->m_type == "DictObject:NumericObject")
    {
        pItem->m_type = "dict"; //pay attention not to export to outside, only as a ui keyword.
        if (!desc.categories.contains("dict"))
            pItem->m_sockProp = SOCKPROP_DICTLIST_PANEL;
    }
    else if (pItem->m_type == "list")
    {
        if (!desc.categories.contains("list"))
            pItem->m_sockProp = SOCKPROP_DICTLIST_PANEL;
    }

    //not type desc on list output socket, add it here.
    if (pItem->m_name == "list" && pItem->m_type.isEmpty())
    {
        pItem->m_type = "list";
        PARAM_CLASS cls = pItem->getParamClass();
        if (cls == PARAM_INPUT && !desc.categories.contains("list"))
            pItem->m_sockProp = SOCKPROP_DICTLIST_PANEL;
    }

    if (pItem->m_sockProp == SOCKPROP_DICTLIST_PANEL)
    {
        DictKeyModel* pDictModel = new DictKeyModel(m_model, pItem->index(), this);
        pItem->m_customData[ROLE_VPARAM_LINK_MODEL] = QVariantPtr<DictKeyModel>::asVariant(pDictModel);
    }
}

void NodeParamModel::onRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    VParamItem* parentItem = static_cast<VParamItem*>(itemFromIndex(parent));
    if (!parentItem || parentItem->vType != VPARAM_GROUP)
        return;

    if (first < 0 || first >= parentItem->rowCount())
        return;

    //todo: begin macro

    VParamItem* pItem = static_cast<VParamItem*>(parentItem->child(first));
    clearLinks(pItem);

    //clear subkeys.
    if (pItem->m_customData.find(ROLE_VPARAM_LINK_MODEL) != pItem->m_customData.end())
    {
        DictKeyModel* keyModel = QVariantPtr<DictKeyModel>::asPtr(pItem->m_customData[ROLE_VPARAM_LINK_MODEL]);
        keyModel->clearAll();
    }
}

bool NodeParamModel::removeRows(int row, int count, const QModelIndex& parent)
{
    VParamItem* parentItem = static_cast<VParamItem*>(itemFromIndex(parent));
    if (!parentItem || parentItem->vType != VPARAM_GROUP)
        return false;

    if (row < 0 || row >= parentItem->rowCount())
        return false;

    VParamItem* pItem = static_cast<VParamItem*>(parentItem->child(row));
    clearLinks(pItem);

    bool ret = ViewParamModel::removeRows(row, count, parent);
    m_pGraphsModel->markDirty();
    return ret;
}

void NodeParamModel::onSubIOEdited(const QVariant& oldValue, const VParamItem* pItem)
{
    if (m_pGraphsModel->IsIOProcessing() || m_bTempModel)
        return;

    const QString& nodeName = m_nodeIdx.data(ROLE_OBJNAME).toString();
    if (nodeName == "SubInput" || nodeName == "SubOutput")
    {
        bool bInput = nodeName == "SubInput";
        const QString& subgName = m_subgIdx.data(ROLE_OBJNAME).toString();

        VParamItem* deflItem = m_params->getItem("defl");
        VParamItem* nameItem = m_params->getItem("name");
        VParamItem* typeItem = m_params->getItem("type");

        ZASSERT_EXIT(deflItem && nameItem && typeItem);
        const QString& sockName = nameItem->m_value.toString();

        if (pItem->m_name == "type")
        {
            const QString& newType = pItem->m_value.toString();
            PARAM_CONTROL newCtrl = UiHelper::getControlByType(newType);
            const QVariant& newValue = UiHelper::initDefaultValue(newType);

            GlobalControlMgr::instance().onParamUpdated(subgName, bInput ? PARAM_INPUT : PARAM_OUTPUT, sockName, newCtrl);

            const QModelIndex& idx_defl = deflItem->index();
            setData(idx_defl, newType, ROLE_PARAM_TYPE);
            setData(idx_defl, newCtrl, ROLE_PARAM_CTRL);
            setData(idx_defl, newValue, ROLE_PARAM_VALUE);

            //update desc.
            NODE_DESC desc;
            bool ret = m_pGraphsModel->getDescriptor(subgName, desc);
            ZASSERT_EXIT(ret);
            if (bInput)
            {
                ZASSERT_EXIT(desc.inputs.find(sockName) != desc.inputs.end());
                desc.inputs[sockName].info.type = newType;
            }
            else
            {
                ZASSERT_EXIT(desc.outputs.find(sockName) != desc.outputs.end());
                desc.outputs[sockName].info.type = newType;
            }
            m_pGraphsModel->updateSubgDesc(subgName, desc);

            //update to every subgraph node.
            QModelIndexList subgNodes = m_pGraphsModel->findSubgraphNode(subgName);
            for (auto idx : subgNodes)
            {
                // update socket for current subgraph node.
                NodeParamModel* nodeParams = QVariantPtr<NodeParamModel>::asPtr(idx.data(ROLE_NODE_PARAMS));
                QModelIndex paramIdx = nodeParams->getParam(bInput ? PARAM_INPUT : PARAM_OUTPUT, sockName);
                nodeParams->setData(paramIdx, newType, ROLE_PARAM_TYPE);
                nodeParams->setData(paramIdx, newCtrl, ROLE_PARAM_CTRL);
                nodeParams->setData(paramIdx, newValue, ROLE_PARAM_VALUE);
            }
        }
        else if (pItem->m_name == "name")
        {
            //1.update desc info for the subgraph node.
            const QString& newName = sockName;
            const QString& oldName = oldValue.toString();

            NODE_DESC desc;
            bool ret = m_pGraphsModel->getDescriptor(subgName, desc);
            ZASSERT_EXIT(ret);
            if (bInput)
            {
                desc.inputs[newName].info.name = newName;
                desc.inputs.remove(oldName);
            }
            else
            {
                desc.outputs[newName].info.name = newName;
                desc.outputs.remove(oldName);
            }
            m_pGraphsModel->updateSubgDesc(subgName, desc);

            //2.update all sockets for all subgraph node.
            QModelIndexList subgNodes = m_pGraphsModel->findSubgraphNode(subgName);
            for (auto idx : subgNodes)
            {
                // update socket for current subgraph node.
                NodeParamModel* nodeParams = QVariantPtr<NodeParamModel>::asPtr(idx.data(ROLE_NODE_PARAMS));
                QModelIndex paramIdx = nodeParams->getParam(bInput ? PARAM_INPUT : PARAM_OUTPUT, oldName);
                nodeParams->setData(paramIdx, newName, ROLE_PARAM_NAME);
            }
        }
        else if (pItem->m_name == "defl")
        {
            const QVariant& deflVal = pItem->m_value;
            NODE_DESC desc;
            bool ret = m_pGraphsModel->getDescriptor(subgName, desc);
            ZASSERT_EXIT(ret);
            if (bInput)
            {
                ZASSERT_EXIT(desc.inputs.find(sockName) != desc.inputs.end());
                desc.inputs[sockName].info.defaultValue = deflVal;
            }
            else
            {
                ZASSERT_EXIT(desc.outputs.find(sockName) != desc.outputs.end());
                desc.outputs[sockName].info.defaultValue = deflVal;
            }
            m_pGraphsModel->updateSubgDesc(subgName, desc);
            //no need to update all subgraph node because it causes disturbance.
        }
    }
}

void NodeParamModel::onLinkAdded(const VParamItem* pItem)
{
    if (m_bTempModel)
        return;

    //dynamic socket from MakeList/Dict and ExtractDict
    QStringList lst = sockNames(PARAM_INPUT);
    int maxObjId = UiHelper::getMaxObjId(lst);
    if (maxObjId == -1)
        maxObjId = 0;

    QString maxObjSock = QString("obj%1").arg(maxObjId);
    QString lastKey = lst.last();
    QString nodeCls = m_nodeIdx.data(ROLE_OBJNAME).toString();
    if ((nodeCls == "MakeList" || nodeCls == "MakeDict") && pItem->m_name == lastKey)
    {
        const QString &newObjName = QString("obj%1").arg(maxObjId + 1);
        SOCKET_PROPERTY prop = nodeCls == "MakeDict" ? SOCKPROP_EDITABLE : SOCKPROP_NORMAL;
        setAddParam(PARAM_INPUT, newObjName, "", QVariant(), CONTROL_NONE, QVariant(), prop);
    }
    else if (nodeCls == "ExtractDict" && pItem->m_name == lastKey)
    {
        const QString &newObjName = QString("obj%1").arg(maxObjId + 1);
        setAddParam(PARAM_OUTPUT, newObjName, "", QVariant(), CONTROL_NONE, QVariant(), SOCKPROP_EDITABLE);
    }
}
