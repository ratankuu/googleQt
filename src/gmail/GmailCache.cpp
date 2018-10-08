#include "GmailRoutes.h"
#include <QSqlError>
#include <QDebug>
#include <ctime>
#include <QDir>
#include "Endpoint.h"
#include "GmailCacheRoutes.h"
#include "gcontact/GcontactCache.h"

#define CONFIG_VERSION "version"

using namespace googleQt;
///MessagesReceiver
mail_cache::MessagesReceiver::MessagesReceiver(GmailRoutes& r, EDataState f) 
    :m_r(r), 
     m_msg_format(f) 
{

};

GoogleTask<messages::MessageResource>* mail_cache::MessagesReceiver::routeRequest(QString message_id)
{   
    gmail::IdArg arg(m_r.endpoint()->client()->userId(), message_id);
    if (m_msg_format == EDataState::snippet)
        {
            arg.setFormat("metadata");
            arg.headers().push_back("Subject");
            arg.headers().push_back("From");
            arg.headers().push_back("To");
            arg.headers().push_back("Cc");
            arg.headers().push_back("Bcc");
        }
    else if (m_msg_format == EDataState::body)
        {
        
        }

#ifdef API_QT_AUTOTEST
    ApiAutotest::INSTANCE().addId("messages::MessageResource", message_id);
#endif
    return m_r.getMessages()->get_Async(arg);
}

///ThreadsReceiver
mail_cache::ThreadsReceiver::ThreadsReceiver(GmailRoutes& r)
	:m_r(r)
{
};

GoogleTask<threads::ThreadResource>* mail_cache::ThreadsReceiver::routeRequest(QString thread_id)
{
	gmail::IdArg arg(m_r.endpoint()->client()->userId(), thread_id);
	arg.setFormat("minimal");

#ifdef API_QT_AUTOTEST
	ApiAutotest::INSTANCE().addId("threads::ThreadResource", thread_id);
#endif
	return m_r.getThreads()->get_Async(arg);
}

///GMailCache
mail_cache::GMailCache::GMailCache(ApiEndpoint& ept)
    :GoogleCache<MessageData, GMailCacheQueryTask>(ept)
{

};

void mail_cache::GMailCache::topCacheData(GMailCacheQueryTask* rfetcher, int number2load, uint64_t labelFilter)
{
    int count = 0;
    for (auto& i : m_ord) {
        auto m = i;
        if (m->inLabelFilter(labelFilter)) {
            rfetcher->add(i);
            if (number2load > 0) {
                if (++count >= number2load)
                    break;
            }
        }
    }
    rfetcher->notifyOnCompletedFromCache();
}

///GThreadCache
mail_cache::GThreadCache::GThreadCache(ApiEndpoint& ept)
    :GoogleCache<ThreadData, GThreadCacheQueryTask>(ept)
{

};

///MessageData
mail_cache::MessageData::MessageData(int accId,
                                     QString id,
								     QString thread_id,
                                     QString from,
                                     QString to,
                                     QString cc,
                                     QString bcc,
                                     QString subject,
                                     QString snippet,
                                     qlonglong internalDate,
                                     qlonglong labels)
    :CacheData(EDataState::snippet, id),
    m_accountId(accId),
	m_thread_id(thread_id),
    m_from(from),
    m_to(to),
    m_cc(cc),
    m_bcc(bcc),
    m_subject(subject),
    m_snippet(snippet),
    m_internalDate(internalDate)     
{
    m_labels = labels;
};

mail_cache::MessageData::MessageData(int accId,
                                     QString id,
									 QString thread_id,
                                     QString from,
                                     QString to,
                                     QString cc,
                                     QString bcc,
                                     QString subject,
                                     QString snippet,
                                     QString plain,
                                     QString html,
                                     qlonglong internalDate,
                                     qlonglong labels)
:CacheData(EDataState::body, id),
	m_accountId(accId),
	m_thread_id(thread_id),
    m_from(from),
    m_to(to),
    m_cc(cc),
    m_bcc(bcc),
    m_subject(subject),
    m_snippet(snippet),
    m_plain(plain),
    m_html(html),
    m_internalDate(internalDate)
{
    m_labels = labels;
};

mail_cache::MessageData::MessageData(int accId,
									 QString thread_id,
                                     int agg_state,
                                     QString id,
                                     QString from,
                                     QString to,
                                     QString cc,
                                     QString bcc,
                                     QString subject,
                                     QString snippet,
                                     QString plain,
                                     QString html,
                                     qlonglong internalDate,
                                     qlonglong labels/*,
									 qreal zoom*/)
:CacheData(EDataState::body, id),
m_accountId(accId),
	m_thread_id(thread_id),
    m_from(from),
    m_to(to),
    m_cc(cc),
    m_bcc(bcc),
    m_subject(subject),
    m_snippet(snippet),
    m_plain(plain),
    m_html(html),
    m_internalDate(internalDate)/*,
	m_zoom(zoom)*/
{
    m_state_agg = agg_state;
    m_labels = labels;
};

void mail_cache::MessageData::updateSnippet(QString from,
                                            QString to,
                                            QString cc,
                                            QString bcc,
                                            QString subject,
                                            QString snippet,
                                            qlonglong labels)
{
    m_state_agg |= static_cast<int>(EDataState::snippet);
    m_from = from;
    m_to = to;
    m_cc = cc;
    m_bcc = bcc;
    m_subject = subject;
    m_snippet = snippet;
    m_labels = labels;
};

void mail_cache::MessageData::updateBody(QString plain, QString html)
{
    m_state_agg |= static_cast<int>(EDataState::body);
    m_plain = plain;
    m_html = html;
};

bool mail_cache::MessageData::inLabelFilter(uint64_t data)const
{
    bool rv = true;
    if (data != 0) {
        rv = (data & m_labels) != 0;
    }
    return rv;
};

bool mail_cache::MessageData::inLabelFilter(const std::set<uint64_t>& ANDfilter)const 
{
    for (auto i = ANDfilter.begin(); i != ANDfilter.end(); i++) {
        if (*i != 0) {
            bool in_filter = (*i & m_labels) != 0;
            if (!in_filter) {
                return false;
            }
        }
    }
    return true;
};

const mail_cache::ATTACHMENTS_LIST& mail_cache::MessageData::getAttachments(GMailSQLiteStorage* storage)
{
    if (!m_attachments.empty()) {
        return m_attachments;
    }

    storage->loadAttachmentsFromDb(*this);
    return m_attachments;
};

void mail_cache::MessageData::merge(CacheData* other)
{
    mail_cache::MessageData* md = dynamic_cast<mail_cache::MessageData*>(other);
    if(!md)
        {
            qWarning() << "ERROR. Expected MessageData";
            return;
        }
    if(m_id != md->m_id)
        {
            qWarning() << "ERROR. Expected ID-identity MessageData" << m_id << md->m_id;
            return;            
        }
    
    if(!isLoaded(EDataState::snippet))
        {
            if(md->isLoaded(EDataState::snippet))
                {
                    m_from = md->from();
                    m_subject = md->subject();
                    m_state_agg |= static_cast<int>(EDataState::snippet);
                }
        }

    if(!isLoaded(EDataState::body))
        {
            if(md->isLoaded(EDataState::body))
                {
                    m_plain = md->plain();
                    m_html = md->html();
                    m_state_agg |= static_cast<int>(EDataState::body);
                }
        }    
};

mail_cache::AttachmentData::AttachmentData() 
{
};

mail_cache::AttachmentData::AttachmentData(QString att_id,
                                           QString mimetype,
                                           QString filename,
                                           quint64 size)
    :m_attachment_id(att_id),
     m_mimetype(mimetype),
     m_filename(filename),
     m_size(size) 
{

};

mail_cache::AttachmentData::AttachmentData(QString att_id,
                                           QString mimetype,
                                           QString filename,
                                           QString local_filename,
                                           quint64 size,
                                           EStatus status)
    :m_attachment_id(att_id),
     m_mimetype(mimetype),
     m_filename(filename),
     m_local_filename(local_filename),
     m_size(size),
     m_status(status)
{

};


mail_cache::LabelData::LabelData() 
{

};

mail_cache::LabelData::LabelData(QString id, 
                                 QString name, 
                                 int mask_base,
                                 bool as_system,
                                 uint64_t unread_messages):
    m_label_id(id), 
    m_label_name(name), 
    m_mask_base(mask_base),
    m_is_system_label(as_system),
    m_unread_messages(unread_messages)
{
    static uint64_t theone = 1;
    m_label_mask = (theone << m_mask_base);
};

bool mail_cache::LabelData::isStarred()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::STARRED), Qt::CaseInsensitive) == 0);
    return rv;
};

bool mail_cache::LabelData::isUnread()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::UNREAD), Qt::CaseInsensitive) == 0);
    return rv;
};

bool mail_cache::LabelData::isSpam()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::SPAM), Qt::CaseInsensitive) == 0);
    return rv;
};

bool mail_cache::LabelData::isTrash()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::TRASH), Qt::CaseInsensitive) == 0);
    return rv;
};

bool mail_cache::LabelData::isDraft()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::DRAFT), Qt::CaseInsensitive) == 0);
    return rv;
};


bool mail_cache::LabelData::isImportant()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::IMPORTANT), Qt::CaseInsensitive) == 0);
    return rv;
};


bool mail_cache::LabelData::isPromotionCategory()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::CATEGORY_PROMOTIONS), Qt::CaseInsensitive) == 0);
    return rv;
};

bool mail_cache::LabelData::isForumsCategory()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::CATEGORY_FORUMS), Qt::CaseInsensitive) == 0);
    return rv;
};

bool mail_cache::LabelData::isUpdatesCategory()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::CATEGORY_UPDATES), Qt::CaseInsensitive) == 0);
    return rv;
};

bool mail_cache::LabelData::isPersonalCategory()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::CATEGORY_PERSONAL), Qt::CaseInsensitive) == 0);
    return rv;
};


bool mail_cache::LabelData::isSocialCategory()const
{
    bool rv = (m_label_id.compare(sysLabelId(SysLabel::CATEGORY_SOCIAL), Qt::CaseInsensitive) == 0);
    return rv;
};

mail_cache::ThreadData::ThreadData(QString id,
	quint64 history_id,
	int		messages_count,
	QString snippet):
	CacheData(EDataState::snippet, id),
	m_history_id(history_id),
	m_messages_count(messages_count),
	m_snippet(snippet)
{

};

void mail_cache::ThreadData::merge(CacheData* other) 
{
	qWarning() << "need implementation";
};

///GMailCacheQueryTask
mail_cache::GMailCacheQueryTask::GMailCacheQueryTask(EDataState state, 
                                                     ApiEndpoint& ept, 
                                                     googleQt::mail_cache::GmailCacheRoutes& r,
                                                     std::shared_ptr<GMailCache> c)
    :CacheQueryTask<MessageData>(state, ept, c), m_r(r)
{

};

void mail_cache::GMailCacheQueryTask::fetchFromCloud_Async(const std::list<QString>& id_list)
{
    if (id_list.empty())
        return;
    
    auto par_runner = m_r.getUserBatchMessages_Async(m_completed->state, id_list);
    
    connect(par_runner, &EndpointRunnable::finished, [=]()
            {
                RESULT_LIST<messages::MessageResource>&& res = par_runner->detachResult();
                for (auto& m : res)
                    {
                        fetchMessage(m.get());
                    }
                std::set<QString> id_set;
                for (std::list<QString>::const_iterator i = id_list.cbegin(); i != id_list.cend(); i++)
                    {
                        id_set.insert(*i);
                    }
                notifyFetchCompleted(m_completed->result_map, id_set);
				auto s = m_r.storage();
				if (s) {
					s->updateMessagesDiagnostic(1, id_set.size());
				}
                par_runner->disposeLater();
            }); 
};

void mail_cache::GMailCacheQueryTask::loadHeaders(messages::MessageResource* m,
                                                  QString& from,
                                                  QString& to,
                                                  QString& cc,
                                                  QString& bcc,
                                                  QString& subject)
{
    auto& header_list = m->payload().headers();
    for (auto& h : header_list)
        {
            if (h.name().compare("From", Qt::CaseInsensitive) == 0)
                {
                    from = h.value();
                }
            else if (h.name().compare("To", Qt::CaseInsensitive) == 0)
                {
                    to = h.value();
                }
            else if (h.name().compare("CC", Qt::CaseInsensitive) == 0)
                {
                    cc = h.value();
                }
            else if (h.name().compare("BCC", Qt::CaseInsensitive) == 0)
                {
                    bcc = h.value();
                }
            else if (h.name().compare("Subject", Qt::CaseInsensitive) == 0)
                {
                    subject = h.value();
                }
        }
};

void mail_cache::GMailCacheQueryTask::loadLabels(messages::MessageResource* m, uint64_t& f)
{
    const std::list <QString>& labels = m->labelids();
    if(labels.size() > 0){
        auto storage = m_r.storage();
        if (storage) {
            f = storage->packLabels(labels);
        }
    }
};

void mail_cache::GMailCacheQueryTask::loadAttachments(messages::MessageResource* m, ATTACHMENTS_LIST& lst)
{
    auto p = m->payload();
    auto& parts = p.parts();
    for (auto& pt : parts) {
        auto& b = pt.body();
        if (b.size() > 0 && !b.attachmentid().isEmpty()) {
            att_ptr att = std::make_shared<AttachmentData>(b.attachmentid(), pt.mimetype(), pt.filename(), b.size());
            lst.push_back(att);
        }
    }
};

void mail_cache::GMailCacheQueryTask::fetchMessage(messages::MessageResource* m)
{
	if (!m_r.storage()) {
		qWarning() << "expected storage";
		return;
	}

    switch (m_completed->state)
        {
        case googleQt::EDataState::snippet:
            {
                uint64_t labels;
                loadLabels(m, labels);
                QString from, to, cc, bcc, subject;
                loadHeaders(m, from, to, cc, bcc, subject);
                msg_ptr md(new MessageData(m_r.storage()->currentAccountId(),
                                           m->id(),
                                           m->threadid(),
                                           from, 
                                           to, 
                                           cc,
                                           bcc,
                                           subject, 
                                           m->snippet(), 
                                           m->internaldate(),
                                           labels));
                //snipped - there will be no attachments here and no body..
                add(md);
            }break;
        case googleQt::EDataState::body:
            {
                QString plain_text, html_text;
                auto p = m->payload();
                if (p.mimetype().compare("text/html") == 0)
                    {
                        QByteArray payload_body = QByteArray::fromBase64(p.body().data(), QByteArray::Base64UrlEncoding);
                        html_text = payload_body.constData();
                        plain_text = html_text;
                        plain_text.remove(QRegExp("<[^>]*>"));
                    }
                else
                    {
                        auto parts = p.parts();
                        for (auto pt : parts)
                            {
                                bool plain_text_loaded = false;
                                bool html_text_loaded = false;
                                auto pt_headers = pt.headers();
                                for (auto h : pt_headers)
                                    {
                                        if (h.name() == "Content-Type") {
                                            if ((h.value().indexOf("text/plain") == 0))
                                                {
                                                    plain_text_loaded = true;
                                                    const messages::MessagePartBody& pt_body = pt.body();
                                                    plain_text = QByteArray::fromBase64(pt_body.data(),
                                                                                        QByteArray::Base64UrlEncoding).constData();
                                                    break;
                                                }
                                            else if ((h.value().indexOf("text/html") == 0))
                                                {
                                                    html_text_loaded = true;
                                                    const messages::MessagePartBody& pt_body = pt.body();
                                                    html_text = QByteArray::fromBase64(pt_body.data(),
                                                                                       QByteArray::Base64UrlEncoding).constData();
                                                    break;
                                                }
                                        }//"Content-Type"
                                    }//pt_headers
                                if (plain_text_loaded && html_text_loaded)
                                    break;
                            }// parts
                    }

                auto i = m_completed->result_map.find(m->id());
                if (i == m_completed->result_map.end())
                    {
                        uint64_t labels;
                        loadLabels(m, labels);
                        QString from, to, cc, bcc, subject;
                        loadHeaders(m, from, to, cc, bcc, subject);
                        std::shared_ptr<MessageData> md(new MessageData(m_r.storage()->currentAccountId(),
                                                                                        m->id(),
																						m->threadid(),
                                                                                        from, 
                                                                                        to, 
                                                                                        cc, 
                                                                                        bcc, 
                                                                                        subject, 
                                                                                        m->snippet(), 
                                                                                        m->internaldate(),
                                                                                        labels));
                        add(md);
                        loadAttachments(m, md->m_attachments);
                        md->updateBody(plain_text, html_text);
                    }
                else
                    {
                        std::shared_ptr<MessageData> md = i->second;
                        if (!md->isLoaded(googleQt::EDataState::snippet)) 
                            {
                                uint64_t labels;
                                loadLabels(m, labels);
                                QString from, to, cc, bcc, subject;
                                loadHeaders(m, from, to, cc, bcc, subject);
                                md->updateSnippet(from, to, cc, bcc, subject, m->snippet(), labels);
                            }
                        loadAttachments(m, md->m_attachments);
                        md->updateBody(plain_text, html_text);
                    }
            }break;//body
        }
};

static bool compare_internalDate(mail_cache::msg_ptr& f,
                                 mail_cache::msg_ptr& s)
{
    return (f->internalDate() > s->internalDate());
};

std::unique_ptr<CacheDataList<mail_cache::MessageData>> mail_cache::GMailCacheQueryTask::waitForResultAndRelease()
{
    if (!isFinished())
        {
            m_in_wait_loop = true;
            waitUntillFinishedOrCancelled();
        }
    m_completed->result_list.sort(compare_internalDate);
    return std::move(m_completed);
};

///GThreadCacheSyncTask
mail_cache::GThreadCacheQueryTask::GThreadCacheQueryTask(googleQt::mail_cache::GmailCacheRoutes& r,
	std::shared_ptr<GThreadCache> c)
	://GoogleVoidTask(r.endpoint()),
	CacheQueryTask<ThreadData>(EDataState::snippet, r.endpoint(), c),
	m_r(r)
{

};

void mail_cache::GThreadCacheQueryTask::fetchFromCloud_Async(const std::list<QString>& id_list)
{
	if (id_list.empty())
		return;

	auto par_runner = m_r.getUserBatchThreads_Async(m_r.endpoint().apiClient()->userId(), id_list);

	connect(par_runner, &EndpointRunnable::finished, [=]()
	{
		RESULT_LIST<threads::ThreadResource>&& res = par_runner->detachResult();
		for (auto& m : res)
		{
		//	fetchMessage(m.get());
		}
		
		/*
		std::set<QString> id_set;
		for (std::list<QString>::const_iterator i = id_list.cbegin(); i != id_list.cend(); i++)
		{
			id_set.insert(*i);
		}
		*/
		///todo - sync data 
		notifyOnFinished();
		//notifyFetchCompleted(m_completed->result_map, id_set);
		par_runner->disposeLater();
	});
};


void mail_cache::GThreadCacheQueryTask::fetchMessage(threads::ThreadResource* t) 
{
	thread_ptr td(new ThreadData(t->id(), t->historyid(), t->messages().size(), t->snipped()));
    add(td);
    //	m_loaded_threads[p->threadId()] = p;
};

///GMailSQLiteStorage
mail_cache::GMailSQLiteStorage::GMailSQLiteStorage(mcache_ptr mc,
                                                   tcache_ptr tc,
                                                   std::shared_ptr<gcontact::GContactCache> cc)
{
    m_msg_cache = mc;
    m_thread_cache = tc;
    m_contact_cache = cc;
};

mail_cache::mcache_ptr mail_cache::GMailSQLiteStorage::lock_mcache()
{
    mail_cache::mcache_ptr c = m_msg_cache.lock();
    if (!c) {
        return c;
    }
    if (!c->isValid()) {
        qWarning() << "ERROR. Invalid cache state. Possible user/context switched." 
                   << c->endpoint().apiClient()->userId();
        c.reset();
        return c;
    }
    return c;
};

mail_cache::tcache_ptr mail_cache::GMailSQLiteStorage::lock_tcache()
{
    mail_cache::tcache_ptr c = m_thread_cache.lock();
    if (!c) {
        return c;
    }
    if (!c->isValid()) {
        qWarning() << "ERROR. Invalid cache state. Possible user/context switched." 
                   << c->endpoint().apiClient()->userId();
        c.reset();
        return c;
    }
    return c;
};


int mail_cache::GMailSQLiteStorage::findAccount(QString userId) 
{
    int accId = -1;
    auto i = m_user2acc.find(userId);
    if (i != m_user2acc.end()) {
        accId = i->second->accountId();
    }
    return accId;
};

QString mail_cache::GMailSQLiteStorage::findUser(int accId)
{
    QString userId;
    auto i = m_id2acc.find(accId);
    if (i != m_id2acc.end()) {
        userId = i->second->userId();
    }
    return userId;
};

mail_cache::acc_ptr mail_cache::GMailSQLiteStorage::currentAccount()
{
	mail_cache::acc_ptr rv = nullptr;
	auto i = m_id2acc.find(m_accId);
	if (i != m_id2acc.end()) {
		rv = i->second;
	}
	return rv;
};

bool mail_cache::GMailSQLiteStorage::ensureMailTables() 
{
    /// generic config ///
    QString sql_config = QString("CREATE TABLE IF NOT EXISTS %1config(config_name TEXT, config_value TEXT)").arg(m_metaPrefix);
    if (!execQuery(sql_config))
        return false;

    if(!reloadDbConfig()){
        return false;
    }

    /// accounts ///
    QString sql_accounts = QString("CREATE TABLE IF NOT EXISTS %1gmail_account(acc_id INTEGER PRIMARY KEY, userid TEXT NOT NULL COLLATE NOCASE, msg_cloud_batch_queried INTEGER, msg_cloud_email_loaded INTEGER)").arg(m_metaPrefix);
    if (!execQuery(sql_accounts))
        return false;

    if (!execQuery(QString("CREATE UNIQUE INDEX IF NOT EXISTS %1acc_userid_idx ON %1gmail_account(userid)").arg(m_metaPrefix)))
        return false;


    /// messages ///
    QString sql_messages = QString("CREATE TABLE IF NOT EXISTS %1gmail_msg(acc_id INTEGER NOT NULL, thread_id TEXT NOT NULL, msg_id TEXT NOT NULL, msg_from TEXT, "
        "msg_to TEXT, msg_cc TEXT, msg_bcc TEXT, msg_subject TEXT, msg_snippet TEXT, msg_plain TEXT, "
        "msg_html TEXT, internal_date INTEGER, msg_state INTEGER, msg_cache_lock INTEGER, msg_labels INTEGER)").arg(m_metaPrefix);
    if (!execQuery(sql_messages))
        return false;

    if (!execQuery(QString("CREATE UNIQUE INDEX IF NOT EXISTS %1msg_accid_idx ON %1gmail_msg(acc_id, msg_id)").arg(m_metaPrefix)))
        return false;


    //// labels ///
    QString sql_labels = QString("CREATE TABLE IF NOT EXISTS %1gmail_label(acc_id INTEGER NOT NULL, label_id TEXT NOT NULL, label_name TEXT, "
        " label_type INTEGER, label_unread_messages INTEGER, label_total_messages INTEGER,"
        " message_list_visibility TEXT, label_list_visibility TEXT, label_mask INTEGER)")
        .arg(m_metaPrefix);
    if (!execQuery(sql_labels))
        return false;

    if (!execQuery(QString("CREATE UNIQUE INDEX IF NOT EXISTS %1lbl_accid_idx ON %1gmail_label(acc_id, label_id)").arg(m_metaPrefix)))
        return false;


    /// attachments ///
    QString sql_att = QString("CREATE TABLE IF NOT EXISTS %1gmail_attachments(att_id TEXT, acc_id INTEGER NOT NULL, msg_id TEXT NOT NULL, file_name TEXT, "
        "local_file_name TEXT, mime_type TEXT, size INTEGER, PRIMARY KEY (att_id, msg_id))")
        .arg(m_metaPrefix);
    if (!execQuery(sql_att))
        return false;

    if (!execQuery(QString("CREATE UNIQUE INDEX IF NOT EXISTS %1att_accid_idx ON %1gmail_attachments(acc_id, msg_id, att_id)").arg(m_metaPrefix)))
        return false;

	//// threads ///
	QString sql_threads = QString("CREATE TABLE IF NOT EXISTS %1gmail_thread(acc_id INTEGER NOT NULL, thread_id TEXT NOT NULL, history_id INTEGER "
		" ,snippet TEXT, messages_count INTEGER, update_time INTEGER)")
		.arg(m_metaPrefix);
	if (!execQuery(sql_threads))
		return false;

	if (!execQuery(QString("CREATE UNIQUE INDEX IF NOT EXISTS %1thread_accid_idx ON %1gmail_thread(acc_id, thread_id)").arg(m_metaPrefix)))
		return false;

    return true;
};

bool mail_cache::GMailSQLiteStorage::init_db(QString dbPath, 
                                             QString downloadPath,
                                             QString contactCachePath,
                                             QString dbName, 
                                             QString db_meta_prefix)
{
    m_accId = -1;
    mcache_ptr mc = lock_mcache();
    if (!mc) {
        return false;
    }

    tcache_ptr tc = lock_tcache();
    if (!tc) {
        return false;
    }    
    
    m_mstorage.reset(new GMessagesStorage(this, mc));
    m_tstorage.reset(new GThreadsStorage(this, tc));
    
    QString userId = mc->endpoint().apiClient()->userId();
    if (userId.isEmpty()) {
        qWarning() << "ERROR. Expected userid (email) for gmail cache";
        return false;
    }

    m_dbPath = dbPath;
    m_downloadDir = downloadPath;
    m_contactCacheDir = contactCachePath;
    m_dbName = dbName;
    m_metaPrefix = db_meta_prefix;

    QDir dest_dir;
    if (!dest_dir.exists(m_downloadDir)) {
        if (!dest_dir.mkpath(m_downloadDir)) {
            qWarning() << "ERROR. Failed to create directory" << m_downloadDir;
            return false;
        };
    }

    if (!dest_dir.exists(m_contactCacheDir)) {
        if (!dest_dir.mkpath(m_contactCacheDir)) {
            qWarning() << "ERROR. Failed to create directory" << m_contactCacheDir;
            return false;
        };
    }

    m_initialized = false;  

    m_db = QSqlDatabase::addDatabase("QSQLITE", dbName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        qWarning() << "ERROR. Failed to connect" << dbName << dbPath;
        return false;
    }

    m_query.reset(new QSqlQuery(m_db));

    if (!ensureMailTables()) {
        qWarning() << "ERROR. Failed to create GMail cache tables" << dbName << dbPath;
        return false;
    }

    auto cc = m_contact_cache.lock();
    if (!cc) {
        qWarning() << "ERROR. Expected GContact cache. Failed to init Gcontact cache tables" << dbName << dbPath;
        return false;
    }


    if (!cc->ensureContactTables()) {
        qWarning() << "ERROR. Failed to create GContacts cache tables" << dbName << dbPath;
        return false;
    }

    /// locate accountID
    reloadDbAccounts();
    m_accId = findAccount(userId);
    if(m_accId == -1)
        {
            QString sql = QString("INSERT INTO %1gmail_account(userid) VALUES(?)").arg(m_metaPrefix);
            QSqlQuery* q = prepareQuery(sql);
            if (!q)return false;
            q->addBindValue(userId);
            if (q->exec()) {
                m_accId = q->lastInsertId().toInt();
                reloadDbAccounts();
            }
        }
    
    m_acc_labels.clear();
    m_acc_maskbase2labels.clear();
	m_threads.clear();
    for (int i = 0; i < 64; i++)
        m_avail_label_base.insert(i);

    if (!loadLabelsFromDb()){
        qWarning() << "ERROR. Failed to load labels from DB";
        return false;
    }

    if (!m_mstorage->loadMessagesFromDb()){
        qWarning() << "ERROR. Failed to load messages from DB";
        return false;
    }

	if (!loadThreadsFromDb()) {
		qWarning() << "ERROR. Failed to load threads from DB";
		return false;
	}


    if(!cc->loadContactsFromDb()){
        qWarning() << "ERROR. Failed to load contacts from DB";
        return false;
    };

    mc->setupLocalStorage(m_mstorage.get());
    
    m_initialized = true;
    return m_initialized;
};

void mail_cache::GMailSQLiteStorage::close_db() 
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_initialized = false;
};




bool mail_cache::GMailSQLiteStorage::loadThreadsFromDb() 
{
	QString sql = QString("SELECT thread_id, history_id, messages_count, snippet FROM %1gmail_thread WHERE acc_id=%2 ORDER BY update_time DESC")
		.arg(m_metaPrefix)
		.arg(m_accId);
	QSqlQuery* q = selectQuery(sql);
	if (!q)
		return false;

	while (q->next())
	{
		QString thread_id = q->value(0).toString();
		quint64 history_id = q->value(1).toULongLong();
		int		messages_count = q->value(2).toInt();
		QString snippet = q->value(3).toString();

		thread_ptr t = std::shared_ptr<ThreadData>(new ThreadData(
			thread_id,
			history_id,
			messages_count,
			snippet));
		m_threads[thread_id] = t;
	}

	return true;
};


static std::map<QString, QString> syslabelID2Name;
static std::map<mail_cache::SysLabel, QString> syslabel2Name;

static std::vector<QString>& getSysLabels()
{
#define ADD_SYS_LABEL(L, N)sys_labels.push_back(#L);syslabelID2Name[#L] = N;syslabel2Name[mail_cache::SysLabel::L] = N;

    static std::vector<QString> sys_labels;
    if(sys_labels.empty()){
        ADD_SYS_LABEL(IMPORTANT, "Important");        
        ADD_SYS_LABEL(CHAT, "Chat");
        ADD_SYS_LABEL(SENT, "Sent");
        ADD_SYS_LABEL(INBOX, "Inbox");
        ADD_SYS_LABEL(TRASH, "Trash");
        ADD_SYS_LABEL(DRAFT, "Draft");
        ADD_SYS_LABEL(SPAM, "Spam");
        ADD_SYS_LABEL(STARRED, "Starred");
        ADD_SYS_LABEL(UNREAD, "Unread");
        ADD_SYS_LABEL(CATEGORY_PERSONAL, "Personal");
        ADD_SYS_LABEL(CATEGORY_SOCIAL, "Social");
        ADD_SYS_LABEL(CATEGORY_FORUMS, "Forum");
        ADD_SYS_LABEL(CATEGORY_UPDATES, "Updates");
        ADD_SYS_LABEL(CATEGORY_PROMOTIONS, "Promotions");        
    }
    return sys_labels;

#undef ADD_SYS_LABEL
}

QString mail_cache::sysLabelId(SysLabel l)
{
    std::vector<QString>& labels_arr = getSysLabels();
    int idx = (int)l;
    if(idx < 0 || idx >= (int)labels_arr.size()){
        qWarning() << "ERROR. Invalid SysLabel index" << idx << labels_arr.size();
        return "";
    }
    
    QString s = getSysLabels()[idx];
    return s;
};

QString mail_cache::sysLabelName(SysLabel l)
{
    if (syslabel2Name.empty()) {
        getSysLabels();
    }

    auto idx = syslabel2Name.find(l);
    if (idx == syslabel2Name.end()) {
        qWarning() << "ERROR. Invalid SysLabel" << (int)l << syslabel2Name.size();
        return "";
    }

    return idx->second;
};

bool mail_cache::GMailSQLiteStorage::loadLabelsFromDb()
{
    QString sql = QString("SELECT label_id, label_name, label_type, label_unread_messages, label_mask FROM %1gmail_label WHERE acc_id=%2 ORDER BY label_id")
        .arg(m_metaPrefix)
        .arg(m_accId);
    QSqlQuery* q = selectQuery(sql);
    if (!q)
        return false;

    std::set<QString> system_labels2ensure;
    std::vector<QString>& sys_labels = getSysLabels();
    for(auto& lbl : sys_labels){
        system_labels2ensure.insert(lbl);
    }
    
    while (q->next())
        {
            QString label_id = q->value(0).toString();
            QString label_name = q->value(1).toString();
            bool label_is_system = (q->value(2).toInt() == 1);
            uint64_t unread_messages = q->value(3).toLongLong();
            int mask_base = q->value(4).toInt();

            if (createAndInsertLabel(label_id,
                                     label_name,
                                     label_is_system,
                                     unread_messages,
                                     mask_base))
                {
                    system_labels2ensure.erase(label_id);
                }
        }

    if (!system_labels2ensure.empty()) {
        for (auto& s : system_labels2ensure) {
            mail_cache::LabelData* lbl = ensureLabel(m_accId, s, true);
            if (!lbl) {
                qWarning() << "ERROR. Failed to create label" << s << m_acc_labels.size();
            }
        }
    }

    return true;
};

bool mail_cache::GMailSQLiteStorage::loadAttachmentsFromDb(MessageData& m) 
{
    int accId = m.accountId();
    if (accId == -1) {
        qWarning() << "ERROR. Invalid accountId" << m.id();
        return false;
    }

    QString sql = QString("SELECT att_id, file_name, local_file_name, mime_type, size FROM %1gmail_attachments WHERE msg_id='%2' AND acc_id=%3 ORDER BY att_id")
        .arg(m_metaPrefix)
        .arg(m.id())
        .arg(accId);
    QSqlQuery* q = selectQuery(sql);
    if (!q)
        return false;

    while (q->next())
        {
            QString att_id = q->value(0).toString();
            QString filename = q->value(1).toString();
            QString local_filename = q->value(2).toString();
            QString mimetype = q->value(3).toString();      
            quint64 size = q->value(4).toInt();
            AttachmentData::EStatus status = AttachmentData::statusNotDownloaded;
            if (!local_filename.isEmpty()) {
                QString local_path = downloadDir() + "/" + local_filename;
                if (QFile::exists(local_path)) {
                    status = AttachmentData::statusDownloaded;
                }
            }

            att_ptr att = std::make_shared<AttachmentData>(att_id, mimetype, filename, local_filename, size, status);
            m.m_attachments.push_back(att);
        }

    return true;
};

bool mail_cache::GMailSQLiteStorage::markMailAsTrashedInDb(MessageData& m) 
{
    int accId = m.accountId();
    if (accId == -1) {
        qWarning() << "ERROR. Invalid accountId" << m.id();
        return false;
    }

    auto lb = findLabel(mail_cache::SysLabel::TRASH);
    if (!lb) {
        qWarning() << "ERROR. Failed to locate label by id" << m.id();
        return false;
    }

    m.m_labels |= lb->labelMask();

    auto sql_update = QString("UPDATE %1gmail_msg SET msg_labels=? WHERE msg_id=? AND acc_id=?")
        .arg(m_metaPrefix);

    QSqlQuery* q = prepareQuery(sql_update);
    if (!q)return false;

    q->addBindValue(static_cast<qint64>(m.labelsBitMap()));
    q->addBindValue(m.id());
    q->addBindValue(accId);

    return true;
};

void mail_cache::GMailSQLiteStorage::updateMessagesDiagnostic(int inc_batch, int inc_msg) 
{
	if (m_accId != -1)
	{
		auto i = m_id2acc.find(m_accId);
		if (i != m_id2acc.end()) {
			auto a = i->second;
			a->m_diagnostic.m_msg_cloud_batch_queried += inc_batch;
			a->m_diagnostic.m_msg_cloud_email_loaded += inc_msg;

			QString sql = QString("UPDATE %1gmail_account SET msg_cloud_batch_queried=%2, msg_cloud_email_loaded=%3 WHERE acc_id = %4")
				.arg(m_metaPrefix)
				.arg(a->m_diagnostic.m_msg_cloud_batch_queried)
				.arg(a->m_diagnostic.m_msg_cloud_email_loaded)
				.arg(m_accId);
			QSqlQuery* q = prepareQuery(sql);
			if (!q)
			{
				qWarning() << "Failed to prepare query" << sql;
				return;
			};
			if (!q->exec())
			{
				qWarning() << "Failed to execute query" << sql;
				return;
			};
		}
	}
};

QString mail_cache::GMailSQLiteStorage::findAttachmentFile(att_ptr att)const 
{
    QString rv;
    if (att->status() == AttachmentData::statusDownloaded &&
        !att->localFilename().isEmpty()) 
        {
            QString local_path = downloadDir() + "/" + att->localFilename();
            if (QFile::exists(local_path)) {
                rv = local_path;
            }
        }
    return rv;
};

void mail_cache::GMailSQLiteStorage::invalidateAttachmentLocalCacheFile(att_ptr att)
{
    att->m_status = AttachmentData::statusNotDownloaded;
    QString local_path = downloadDir() + "/" + att->localFilename();
    if (QFile::exists(local_path)) {
        bool ok = QFile::remove(local_path);
        if(!ok){
            qWarning() << "failed to delete cache file" << local_path;
        }
    }
};


mail_cache::LabelData* mail_cache::GMailSQLiteStorage::createAndInsertLabel(
                                                                            QString label_id,
                                                                            QString label_name,
                                                                            bool label_is_system,
                                                                            uint64_t unread_messages,
                                                                            int mask_base
                                                                            ) 
{
    label_ptr lb(new LabelData(label_id,
                               label_name,
                               mask_base,
                               label_is_system,
                               unread_messages));

    m_avail_label_base.erase(mask_base);
    m_acc_labels[lb->labelId()] = lb;
    m_acc_maskbase2labels[mask_base] = lb;
    return lb.get();
};

bool mail_cache::GMailSQLiteStorage::updateDbLabel(const labels::LabelResource& lbl)
{
    QString name = lbl.name();
    if(lbl.type().compare("system", Qt::CaseInsensitive) == 0){
        auto i = syslabelID2Name.find(lbl.id());
        if (i != syslabelID2Name.end()) {
            name = i->second;
        }        
    }
    
    QString sql_update;
    sql_update = QString("UPDATE %1gmail_label SET label_name='%2', label_unread_messages=%3 WHERE label_id='%4' AND acc_id=%5")
        .arg(m_metaPrefix)
        .arg(name)
        .arg(lbl.messagesunread())
        .arg(lbl.id())
        .arg(m_accId);
    return execQuery(sql_update);
};


mail_cache::LabelData* mail_cache::GMailSQLiteStorage::insertDbLabel(const labels::LabelResource& lbl) 
{
    if (m_avail_label_base.empty())
        {
            qWarning() << "ERROR. Exhausted labels masks. New label can not be registered." << m_avail_label_base.size() << lbl.id() << lbl.name();
            return nullptr;
        }

    auto i = m_avail_label_base.begin();
    int mask_base = *i;

    QString sql_insert;
    sql_insert = QString("INSERT INTO  %1gmail_label(acc_id, label_id, label_name, label_type, label_unread_messages,"
                         "label_total_messages, message_list_visibility, label_list_visibility, label_mask) VALUES(%2, ?, ?, ?, ?, ?, ?, ?, ?)")
        .arg(m_metaPrefix)
        .arg(m_accId);
    QSqlQuery* q = prepareQuery(sql_insert);
    if (!q)return nullptr;

    int label_type_as_int = 2;
    if (lbl.type().compare("system", Qt::CaseInsensitive) == 0) {
        label_type_as_int = 1;
    }

    q->addBindValue(lbl.id());
    q->addBindValue(lbl.name());
    q->addBindValue(label_type_as_int);
    q->addBindValue(lbl.messagesunread());
    q->addBindValue(lbl.messagestotal());
    q->addBindValue(lbl.messagelistvisibility());
    q->addBindValue(lbl.labellistvisibility());
    q->addBindValue(mask_base);

    mail_cache::LabelData* rv = nullptr;

    if (q->exec())
        {
            rv = createAndInsertLabel(lbl.id(),
                                      lbl.name(),
                                      (label_type_as_int == 1),
                                      lbl.messagesunread(),
                                      mask_base);
        }
    else {
        qWarning() << "ERROR. Failed to store label" << sql_insert;

    }

    return rv;
};

mail_cache::LabelData* mail_cache::GMailSQLiteStorage::insertDbLabel(int accId, 
                                                                     QString label_id,
                                                                     QString label_name,
                                                                     QString label_type) 
{
    if (m_avail_label_base.empty()) 
        {
            qWarning() << "ERROR. Exhausted labels masks. New label can not be registered." << m_avail_label_base.size() << label_id << label_name;
            return nullptr;
        }

    auto i = m_avail_label_base.begin();
    int mask_base = *i;

    QString sql_insert;
    sql_insert = QString("INSERT INTO  %1gmail_label(acc_id, label_id, label_name, label_type, label_mask) VALUES(%2, ?, ?, ?, ?)")
        .arg(m_metaPrefix)
        .arg(accId);
    QSqlQuery* q = prepareQuery(sql_insert);
    if (!q)return nullptr;

    int label_type_as_int = 2;
    if (label_type.compare("system", Qt::CaseInsensitive) == 0) {
        label_type_as_int = 1;
    }

    q->addBindValue(label_id);
    q->addBindValue(label_name);
    q->addBindValue(label_type_as_int);
    q->addBindValue(mask_base);

    mail_cache::LabelData* rv = nullptr;

    if (q->exec())
        {
            rv = createAndInsertLabel(label_id,
                                      label_name,
                                      (label_type_as_int == 1),
                                      0,
                                      mask_base);
        }
    else {
        qWarning() << "ERROR. Failed to store label" << sql_insert << q->lastError().text();
    }

    return rv;
};

bool mail_cache::GMailSQLiteStorage::deleteAttachmentsFromDb(QString msg_id)
{
    QString sql = QString("DELETE FROM %1gmail_attachments WHERE msg_id IN('%2')")
        .arg(m_metaPrefix)
        .arg(msg_id);
    QSqlQuery* q = prepareQuery(sql);
    if (!q)return false;
    if (!q->exec()) return false;
    return true;
};

void mail_cache::GMailSQLiteStorage::reloadDbAccounts() 
{
    m_id2acc.clear();
    m_user2acc.clear();

    QString sql = QString("SELECT acc_id, userid, msg_cloud_batch_queried, msg_cloud_email_loaded FROM %1gmail_account").arg(m_metaPrefix);
    QSqlQuery* q = selectQuery(sql);
    if (!q) {
        qWarning() << "ERROR. Failed to load account data from DB" << sql;
        return ;
    }

    while (q->next())
    {
        int acc_id = q->value(0).toInt();
        QString userid = q->value(1).toString();
		int msg_cloud_batch_queried = q->value(2).toInt();
		int msg_cloud_email_loaded = q->value(3).toInt();
        mail_cache::acc_ptr p(new AccountData(acc_id, userid));
        m_id2acc[acc_id] = p;
        m_user2acc[userid] = p;

		p->m_diagnostic.m_msg_cloud_batch_queried = msg_cloud_batch_queried;
		p->m_diagnostic.m_msg_cloud_email_loaded = msg_cloud_email_loaded;
    }
};

bool mail_cache::GMailSQLiteStorage::reloadDbConfig()
{
    m_configs.clear();
    QString sql = QString("SELECT config_name, config_value FROM %1config")
        .arg(m_metaPrefix);
    QSqlQuery* q = selectQuery(sql);
    if (!q)
        return false;
    
    int db_version = 0;

    while (q->next())
    {
        QString name = q->value(0).toString();
        QString value = q->value(1).toString();
        m_configs[name] = value;

       if (name.compare(CONFIG_VERSION, Qt::CaseInsensitive) == 0) {
           db_version = value.toInt();
        }
    }

    if(db_version == 0){
        QString sql = QString("INSERT OR REPLACE INTO %1config(config_name, config_value) VALUES(%2, %3)")
            .arg(m_metaPrefix)
            .arg("'version'")
            .arg('1');
        QSqlQuery* q = prepareQuery(sql);
        if (!q){
            qWarning() << "ERROR. Failed to prepare query for cache version setup";
            return false;
        }
        if (!q->exec()) {
            qWarning() << "ERROR. Failed to setup cache DB version";
            return false;
        }
    }

    return true;
};

std::list<mail_cache::acc_ptr> mail_cache::GMailSQLiteStorage::getAccounts()
{
    std::list<mail_cache::acc_ptr> rv;
    for (auto& i : m_user2acc) {
        rv.push_back(i.second);
    }
    return rv;
};

bool mail_cache::GMailSQLiteStorage::deleteAccountFromDb(int accId) 
{
    QString sql = QString("DELETE FROM %1gmail_msg WHERE acc_id = %2")
        .arg(m_metaPrefix)
        .arg(accId);
    bool ok = execQuery(sql);
    if(!ok){
        qWarning() << "ERROR.Failed to delete messages" << sql;
        return false;
    }

    sql = QString("DELETE FROM %1gmail_label WHERE acc_id = %2")
        .arg(m_metaPrefix)
        .arg(accId);
    ok = execQuery(sql);
    if(!ok){
        qWarning() << "ERROR.Failed to delete labels" << sql;
        return false;
    }

    sql = QString("DELETE FROM %1gmail_attachments WHERE acc_id = %2")
        .arg(m_metaPrefix)
        .arg(accId);
    ok = execQuery(sql);
    if(!ok){
        qWarning() << "ERROR.Failed to delete attachments" << sql;
        return false;
    }
    
    sql = QString("DELETE FROM %1gmail_account WHERE acc_id = %2")
        .arg(m_metaPrefix)
        .arg(accId);
    ok = execQuery(sql);
    if(!ok){
        qWarning() << "ERROR.Failed to delete account" << sql;
        return false;
    }    
    reloadDbAccounts();
    return true;
};

bool mail_cache::GMailSQLiteStorage::canRenameAccount(int accId)
{
    QString sql = QString("SELECT count(*) n FROM %1gmail_msg WHERE acc_id = %2"
                          " UNION "
                          "SELECT count(*) n FROM %1gmail_label WHERE acc_id = %2"
                          " UNION "
                          "SELECT count(*) n FROM %1gmail_attachments WHERE acc_id = %2")
        .arg(m_metaPrefix)
        .arg(accId);
    QSqlQuery* q = selectQuery(sql);
    if (!q){
        qWarning() << "ERROR. Failed to prepare SQL" << sql;
        return false;
    }
    while (q->next())
        {
            int num = q->value(0).toInt();
            if(num > 0)
                return false;
        }

    return true;
};

bool mail_cache::GMailSQLiteStorage::updateAccountInDb(int accId, QString userId) 
{
    if(!canRenameAccount(accId)){
        return false;
    }
    
    QString sql = QString("UPDATE %1gmail_account SET userid='%2' WHERE acc_id = %3")
        .arg(m_metaPrefix)
        .arg(userId)
        .arg(accId);
    QSqlQuery* q = prepareQuery(sql);
    if (!q)return false;
    if (!q->exec()) return false;
    reloadDbAccounts();
    return true;
};

int mail_cache::GMailSQLiteStorage::addAccountDb(QString userId) 
{
    int accId = -1;
    QString sql = QString("INSERT INTO %1gmail_account(userid) VALUES(?)").arg(m_metaPrefix);
    QSqlQuery* q = prepareQuery(sql);
    if (!q)return -1;
    q->addBindValue(userId);
    if (q->exec()) {
        accId = q->lastInsertId().toInt();
        reloadDbAccounts();
    }
    return accId;
};

mail_cache::LabelData* mail_cache::GMailSQLiteStorage::findLabel(QString label_id)
{
    mail_cache::LabelData* rv = nullptr;
    auto i = m_acc_labels.find(label_id);
    if (i != m_acc_labels.end()) {
        rv = i->second.get();
    }
    return rv;
};

mail_cache::LabelData* mail_cache::GMailSQLiteStorage::findLabel(SysLabel sys_label)
{
    QString lable_id = sysLabelId(sys_label);
    return findLabel(lable_id);
};

mail_cache::LabelData* mail_cache::GMailSQLiteStorage::ensureLabel(int accId, QString label_id, bool system_label)
{
    auto i = m_acc_labels.find(label_id);
    if (i != m_acc_labels.end()) {
        return i->second.get();
    }

    QString label_name = label_id;
    if (system_label) {
        auto i = syslabelID2Name.find(label_id);
        if (i != syslabelID2Name.end()) {
            label_name = i->second;
        }
    }

    mail_cache::LabelData* rv = insertDbLabel(accId,
                                              label_id,
                                              label_name,
                                              system_label ? "system" : "user");
    return rv;
};

std::list<mail_cache::LabelData*> mail_cache::GMailSQLiteStorage::getLabelsInSet(std::set<QString>* in_optional_idset /*= nullptr*/)
{
    std::list<mail_cache::LabelData*> rv;
    for (auto& i : m_acc_labels) {
        bool add_label = true;
        if (in_optional_idset) {
            add_label = (in_optional_idset->find(i.first) != in_optional_idset->end());
        }
        if (add_label) {
            rv.push_back(i.second.get());
        }
    }
    return rv;
};

uint64_t mail_cache::GMailSQLiteStorage::packLabels(const std::list <QString>& labels)
{
    uint64_t f = 0;

    if (labels.size() > 0) {
        for (auto& label_id : labels) {
            auto i = m_acc_labels.find(label_id);
            if (i != m_acc_labels.end()) {
                auto lb = i->second.get();
                f |= lb->labelMask();
            }
            else {
                static int warning_count = 0;
                if (warning_count < 100)
                    {
                        qWarning() << "ERROR. Lost label (pack)" << label_id << m_acc_labels.size();
                        warning_count++;
                    }
            }
        }
    }

    return f;
};

std::list<mail_cache::LabelData*> mail_cache::GMailSQLiteStorage::unpackLabels(const uint64_t& data)const
{
    std::list<mail_cache::LabelData*> labels;

    uint64_t theone = 1;
    for (int b = 0; b < 64; b++)
        {
            uint64_t m = (theone << b);
            if (m > data)
                break;
            if (m & data) {
                auto i = m_acc_maskbase2labels.find(b);
                if (i != m_acc_maskbase2labels.end()){
                    labels.push_back(i->second.get());
                }
                else {
                    static int warning_count = 0;
                    if (warning_count < 100)
                        {
                            qWarning() << "ERROR. Lost label (unpack)" << b << m_acc_maskbase2labels.size();
                            warning_count++;
                        }
                }

            }
        }

    return labels;
};

void mail_cache::GMailSQLiteStorage::update_message_labels_db(int accId, QString msg_id, uint64_t flags)
{
    QString sql_update;
    sql_update = QString("UPDATE %1gmail_msg SET msg_labels=%2 WHERE msg_id='%3' AND acc_id=%4")
        .arg(m_metaPrefix)
        .arg(flags)
        .arg(msg_id)
        .arg(accId);
    execQuery(sql_update);
};

void mail_cache::GMailSQLiteStorage::update_attachment_local_file_db(googleQt::mail_cache::msg_ptr m, 
                                                                     googleQt::mail_cache::att_ptr a, 
                                                                     QString file_name)
{
    int accId = m->accountId();
    if (accId == -1) {
        qWarning() << "ERROR. Failed to locate accountId" << m->id();
        return;
    }

    QString sql_update;
    sql_update = QString("UPDATE %1gmail_attachments SET local_file_name='%2' WHERE msg_id='%3' AND att_id='%4' AND acc_id=%5")
        .arg(m_metaPrefix)
        .arg(file_name)
        .arg(m->id())
        .arg(a->attachmentId())
        .arg(accId);
    bool ok = execQuery(sql_update);
    if (ok) {
        a->m_local_filename = file_name;
        a->m_status = AttachmentData::statusDownloaded;
    }
};

bool mail_cache::GMailSQLiteStorage::execQuery(QString sql)
{
    if(!m_query){
            qWarning() << "ERROR. Expected internal query";
            return false;
        }
    
    if(!m_query->prepare(sql)){
            QString error = m_query->lastError().text();
            qWarning() << "ERROR. Failed to prepare sql query" 
                       << error 
                       << sql;
            return false;
        };    
    if(!m_query->exec(sql)){
            QString error = m_query->lastError().text();
            qWarning() << "ERROR. Failed to execute query" 
                       << error 
                       << sql;
            return false;
        }
    else {
        /*
#ifdef _DEBUG
        qDebug() << "db-affected" << m_query->numRowsAffected();
        if (m_query->numRowsAffected() == 0 && sql.indexOf("CREATE") == -1) {
            qDebug() << "last0rows" << sql;
        }
#endif
*/
    }

    return true;
};

QSqlQuery* mail_cache::GMailSQLiteStorage::prepareQuery(QString sql)
{
    if (!m_query)
        {
            qWarning() << "ERROR. Expected internal query";
            return nullptr;
        }
    if (!m_query->prepare(sql))
        {
            QString error = m_query->lastError().text();
            qWarning() << "ERROR. Failed to prepare sql query" << error << sql;
            return nullptr;
        };
    return m_query.get();
};

QSqlQuery* mail_cache::GMailSQLiteStorage::selectQuery(QString sql)
{
    QSqlQuery* q = prepareQuery(sql);
    if (!q)return nullptr;
    if(!q->exec(sql))
        {
            QString error = q->lastError().text();
            qWarning() << "ERROR. Failed to execute query" << error << sql;
            return nullptr;
        };
    return q;
};

QString mail_cache::GMailSQLiteStorage::lastSqlError()const 
{
	if (!m_query) {
		return "";
	}
	QString error = m_query->lastError().text();
	return error;
};

/**
	GMessagesStorage
*/
mail_cache::GMessagesStorage::GMessagesStorage(GMailSQLiteStorage* s, mcache_ptr c)
{
	m_storage = s;
	m_cache = c;
};

std::list<QString> mail_cache::GMessagesStorage::load_db(EDataState state,
	const std::list<QString>& id_list,
	GMailCacheQueryTask* cr)
{
	std::list<QString> rv;

	auto cache = m_cache.lock();
	if (!cache) {
		qWarning() << "cache lock failed";
		return rv;
	}

	std::set<QString> db_loaded;
	std::function<bool(const std::list<QString>& lst)> loadSublist = [&](const std::list<QString>& lst) -> bool
	{
		QString comma_ids = slist2commalist_decorated(lst);
		QString sql = QString("SELECT msg_state, msg_id, thread_id, msg_from, msg_to, msg_cc, msg_bcc, "
			"msg_subject, msg_snippet, msg_plain, msg_html, internal_date, msg_labels FROM %1gmail_msg WHERE msg_id IN(%2) AND acc_id=%3")
			.arg(m_storage->metaPrefix())
			.arg(comma_ids)
			.arg(m_storage->currentAccountId());
		switch (state)
		{
		case EDataState::snippet:
		{
			sql += " AND (msg_state = 1 OR msg_state = 3)";
		}break;
		case EDataState::body:
		{
			sql += " AND (msg_state = 2 OR msg_state = 3)";
		}break;
		}
		sql += " ORDER BY internal_date DESC";
		QSqlQuery* q = m_storage->selectQuery(sql);
		if (!q)
			return false;
		while (q->next())
		{
			auto md = loadMessage(q);
			if (md)
			{
				cache->mem_insert(md->id(), md);
				db_loaded.insert(md->id());
				cr->add(md);
			}
		}
		return true;
	};


	if (isValid())
	{
		if (chunk_list_execution(id_list, loadSublist))
		{
			for (std::list<QString>::const_iterator i = id_list.begin(); i != id_list.end(); i++)
			{
				auto j = db_loaded.find(*i);
				if (j == db_loaded.end())
				{
					rv.push_back(*i);
				}
			}
		};
	}
	return rv;
};

mail_cache::msg_ptr mail_cache::GMessagesStorage::loadMessage(QSqlQuery* q)
{
	mail_cache::msg_ptr md;

	int agg_state = q->value(0).toInt();
	if (agg_state < 1 || agg_state > 3)
	{
		qWarning() << "ERROR. Invalid DB state" << agg_state << q->value(1).toString();
		return nullptr;
	}
	QString msg_id = q->value(1).toString();
	if (msg_id.isEmpty())
	{
		qWarning() << "ERROR. Invalid(empty) msg_id";
		return nullptr;
	}

	QString thread_id = q->value(2).toString();
	if (thread_id.isEmpty())
	{
		qWarning() << "ERROR. Invalid(empty) thread_id";
		return nullptr;
	}


	QString msg_from = q->value(3).toString();
	QString msg_to = q->value(4).toString();
	QString msg_cc = q->value(5).toString();
	QString msg_bcc = q->value(6).toString();
	QString msg_subject = q->value(7).toString();
	QString msg_snippet = q->value(8).toString();
	QString msg_plain = "";
	QString msg_html = "";

	if (agg_state > 1)
	{
		msg_plain = q->value(9).toString();
		msg_html = q->value(10).toString();
	}
	qlonglong  msg_internalDate = q->value(11).toLongLong();
	qlonglong  msg_labels = q->value(12).toLongLong();

	md = std::shared_ptr<MessageData>(new MessageData(m_storage->currentAccountId(),
		thread_id,
		agg_state,
		msg_id,
		msg_from,
		msg_to,
		msg_cc,
		msg_bcc,
		msg_subject,
		msg_snippet,
		msg_plain,
		msg_html,
		msg_internalDate,
		msg_labels));
	return md;
};

void mail_cache::GMessagesStorage::update_db(
	EDataState state,
	CACHE_QUERY_RESULT_LIST<mail_cache::MessageData>& r)
{
	QString sql_update, sql_insert;
	switch (state)
	{
	case EDataState::snippet:
	{
		sql_update = QString("UPDATE %1gmail_msg SET msg_state=?, msg_from=?, msg_to=?, "
			"msg_cc=?, msg_bcc=?, msg_subject=?, msg_snippet=?, internal_date=?, msg_labels=? WHERE msg_id=? AND acc_id=? AND thread_id=?")
			.arg(m_storage->metaPrefix());
		sql_insert = QString("INSERT INTO %1gmail_msg(msg_state, msg_from, msg_to, msg_cc, msg_bcc, msg_subject, "
			"msg_snippet, internal_date, msg_labels, msg_id, acc_id, thread_id) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
			.arg(m_storage->metaPrefix());
	}break;
	case EDataState::body:
	{
		sql_update = QString("UPDATE %1gmail_msg SET msg_state=?, msg_from=?, msg_to=?, msg_cc=?, msg_bcc=?, "
			"msg_subject=?, msg_snippet=?, msg_plain=?, msg_html=?, internal_date=?, msg_labels=? WHERE msg_id=? AND acc_id=? AND thread_id=?")
			.arg(m_storage->metaPrefix());
		sql_insert = QString("INSERT INTO %1gmail_msg(msg_state, msg_from, msg_to, msg_cc, msg_bcc, msg_subject, "
			"msg_snippet, msg_plain, msg_html, internal_date, msg_labels, msg_id, acc_id, thread_id) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
			.arg(m_storage->metaPrefix());
	}break;
	}

	std::function<bool(QSqlQuery*, mail_cache::MessageData*)> execWithValues =
		[&](QSqlQuery* q,
			mail_cache::MessageData* m) -> bool
	{
		switch (state)
		{
		case EDataState::snippet:
		{
			q->addBindValue(m->aggState());
			q->addBindValue(m->from());
			q->addBindValue(m->to());
			q->addBindValue(m->cc());
			q->addBindValue(m->bcc());
			q->addBindValue(m->subject());
			q->addBindValue(m->snippet());
			q->addBindValue(m->internalDate());
			q->addBindValue(static_cast<qint64>(m->labelsBitMap()));
			q->addBindValue(m->id());
			q->addBindValue(m_storage->currentAccountId());
			q->addBindValue(m->threadId());
		}break;
		case EDataState::body:
		{
			q->addBindValue(m->aggState());
			q->addBindValue(m->from());
			q->addBindValue(m->to());
			q->addBindValue(m->cc());
			q->addBindValue(m->bcc());
			q->addBindValue(m->subject());
			q->addBindValue(m->snippet());
			q->addBindValue(m->plain());
			q->addBindValue(m->html());
			q->addBindValue(m->internalDate());
			q->addBindValue(static_cast<qint64>(m->labelsBitMap()));
			q->addBindValue(m->id());
			q->addBindValue(m_storage->currentAccountId());
			q->addBindValue(m->threadId());
		}break;
		}
		return q->exec();
	};

	int updated_records = 0;
	int inserted_records = 0;

	for (auto& i : r)
	{
		msg_ptr& m = i;
		QSqlQuery* q = m_storage->prepareQuery(sql_update);
		if (!q)return;
		bool ok = execWithValues(q, m.get());
		if (!ok)
		{
			qWarning() << "ERROR. SQL update failed" << q->lastError().text() << i->id();
		}
		int rows_updated = q->numRowsAffected();
		if (rows_updated <= 0)
		{
			q = m_storage->prepareQuery(sql_insert);
			if (!q)return;
			ok = execWithValues(q, m.get());
			if (!ok)
			{
				qWarning() << "ERROR. SQL insert failed" << q->lastError().text() << i->id();
			}
			else
			{
				int rows_affected = q->numRowsAffected();
				if (rows_affected > 0)
					inserted_records++;
				int att_count = m->m_attachments.size();
				if (att_count > 0) {
					insertDbAttachmentData(*m.get());
				}
			}
		}
		else
		{
			updated_records++;
			int att_count = m->m_attachments.size();
			if (att_count > 0) {
				insertDbAttachmentData(*m.get());
			}
		}
	}
};


void mail_cache::GMessagesStorage::insertDbAttachmentData(const mail_cache::MessageData& m)
{
	if (m.m_attachments.size() == 0) {
		qWarning() << "ERROR. Not attachments to store for" << m.id();
		return;
	}

	int accId = m.accountId();
	if (accId == -1) {
		qWarning() << "ERROR. Invalid accountId" << m.id();
		return;
	}

	QString sql_insert;
	sql_insert = QString("INSERT INTO  %1gmail_attachments(att_id, acc_id, msg_id, file_name, mime_type, size)"
		" VALUES(?, %2, ?, ?, ?, ?)")
		.arg(m_storage->metaPrefix())
		.arg(accId);

	for (auto& a : m.m_attachments)
	{
		QSqlQuery* q = m_storage->prepareQuery(sql_insert);
		if (!q) {
			QString error = m_storage->lastSqlError();
			qWarning() << "ERROR. Failed to prepare SQL" << sql_insert << error;
			return;
		}

		q->addBindValue(a->attachmentId());
		q->addBindValue(m.id());
		q->addBindValue(a->filename());
		q->addBindValue(a->mimetype());
		q->addBindValue(a->size());

		if (!q->exec()) {
			QString error = m_storage->lastSqlError();
			qWarning() << "ERROR. Failed to store attachment accId=" << m_storage->currentAccountId() << m.id() << a->attachmentId() << error;
			continue;
		}
	}
};

void mail_cache::GMessagesStorage::remove_db(const std::set<QString>& set_of_ids2remove)
{
	std::list<QString> ids2remove;
	for (auto& i : set_of_ids2remove)
	{
		ids2remove.push_back(i);
	}

	std::function<bool(const std::list<QString>& lst)> removeSublist = [&](const std::list<QString>& lst) -> bool
	{
		QString comma_ids = slist2commalist_decorated(lst);
		QString sql = QString("DELETE FROM %1gmail_msg WHERE msg_id IN(%2) AND acc_id=%3")
			.arg(m_storage->metaPrefix())
			.arg(comma_ids)
			.arg(m_storage->currentAccountId());
		QSqlQuery* q = m_storage->prepareQuery(sql);
		if (!q)return false;
		if (!q->exec()) return false;
		return true;
	};

	if (isValid())
	{
		if (!chunk_list_execution(ids2remove, removeSublist))
		{
			qWarning() << "ERROR. Failed to remove list of records.";
		}
	}
};

bool mail_cache::GMessagesStorage::isValid()const 
{
	return m_storage->isValid();
};

bool mail_cache::GMessagesStorage::loadMessagesFromDb()
{
    mcache_ptr cache = m_cache.lock();
    if (!cache) {
        return false;
    }

    QString sql = QString("SELECT msg_state, msg_id, thread_id, msg_from, msg_to, msg_cc, msg_bcc, "
                          "msg_subject, msg_snippet, msg_plain, msg_html, internal_date, msg_labels FROM %1gmail_msg WHERE acc_id=%2 ORDER BY internal_date DESC")
        .arg(m_storage->metaPrefix())
        .arg(m_storage->currentAccountId());
    QSqlQuery* q = m_storage->selectQuery(sql);
    if (!q)
        return false;

    int loaded_objects = 0;
    while (q->next())
        {
            std::shared_ptr<MessageData> md = loadMessageFromDB(q);
            if (md == nullptr)
                continue;
			cache->mem_insert(md->id(), md);
            loaded_objects++;
        }

    
#ifdef API_QT_AUTOTEST
    ApiAutotest::INSTANCE() << QString("DB-loaded %1 records, mem-cache-size: %3")
        .arg(loaded_objects).arg(cache->mem_size());
#endif


    return true;
};

mail_cache::msg_ptr mail_cache::GMessagesStorage::loadMessageFromDB(QSqlQuery* q)
{
    mail_cache::msg_ptr md;

    int agg_state = q->value(0).toInt();
    if (agg_state < 1 || agg_state > 3)
        {
            qWarning() << "ERROR. Invalid DB state" << agg_state << q->value(1).toString();
            return nullptr;
        }
    QString msg_id = q->value(1).toString();
    if (msg_id.isEmpty())
        {
            qWarning() << "ERROR. Invalid(empty) msg_id";
            return nullptr;
        }

	QString thread_id = q->value(2).toString();
	if (thread_id.isEmpty())
	{
		qWarning() << "ERROR. Invalid(empty) thread_id";
		return nullptr;
	}


    QString msg_from = q->value(3).toString();
    QString msg_to = q->value(4).toString();
    QString msg_cc = q->value(5).toString();
    QString msg_bcc = q->value(6).toString();
    QString msg_subject = q->value(7).toString();
    QString msg_snippet = q->value(8).toString();
    QString msg_plain = "";
    QString msg_html = "";
    
    if (agg_state > 1)
        {
            msg_plain = q->value(9).toString();
            msg_html = q->value(10).toString();
        }
    qlonglong  msg_internalDate = q->value(11).toLongLong();
    qlonglong  msg_labels = q->value(12).toLongLong();

    md = std::shared_ptr<MessageData>(new MessageData(m_storage->currentAccountId(),
													  thread_id,
                                                      agg_state, 
                                                      msg_id, 
                                                      msg_from, 
                                                      msg_to, 
                                                      msg_cc,
                                                      msg_bcc,
                                                      msg_subject, 
                                                      msg_snippet, 
                                                      msg_plain, 
                                                      msg_html, 
                                                      msg_internalDate,
                                                      msg_labels));
    return md;
};

/**
	GThreadsStorage
*/
mail_cache::GThreadsStorage::GThreadsStorage(GMailSQLiteStorage* s, tcache_ptr c)
{
	m_storage = s;
	m_cache = c;
};

std::list<QString> mail_cache::GThreadsStorage::load_db(EDataState, const std::list<QString>& id_list, GThreadCacheQueryTask* cr)
{
	std::list<QString> rv;

	auto cache = m_cache.lock();
	if (!cache) {
		qWarning() << "cache lock failed";
		return rv;
	}

	std::set<QString> db_loaded;
	std::function<bool(const std::list<QString>& lst)> loadSublist = [&](const std::list<QString>& lst) -> bool
	{
		QString comma_ids = slist2commalist_decorated(lst);
		QString sql = QString("SELECT thread_id, history_id, messages_count, snippet "
			"FROM %1gmail_thread WHERE thread_id IN(%2) AND acc_id=%3")
			.arg(m_storage->metaPrefix())
			.arg(comma_ids)
			.arg(m_storage->currentAccountId());

        sql += " ORDER BY internal_date DESC";
        QSqlQuery* q = m_storage->selectQuery(sql);
        if (!q)
            return false;    

		while (q->next())
		{
			auto td = loadThread(q);
			if (td)
			{
				cache->mem_insert(td->id(), td);
				db_loaded.insert(td->id());
				cr->add(td);
			}
		}
		return true;        
    };

	if (isValid())
	{
		if (chunk_list_execution(id_list, loadSublist))
		{
			for (std::list<QString>::const_iterator i = id_list.begin(); i != id_list.end(); i++)
			{
				auto j = db_loaded.find(*i);
				if (j == db_loaded.end())
				{
					rv.push_back(*i);
				}
			}
		};
	}
	return rv;    
};

mail_cache::thread_ptr mail_cache::GThreadsStorage::loadThread(QSqlQuery* q)
{
    mail_cache::thread_ptr td;

	QString thread_id = q->value(0).toString();
	if (thread_id.isEmpty())
	{
		qWarning() << "ERROR. Invalid(empty) thread_id";
		return nullptr;
	}
    
	quint64 history_id = q->value(1).toULongLong();
    int messages_count = q->value(2).toInt();
    QString snippet = q->value(3).toString();
    
    td = std::shared_ptr<ThreadData>(new ThreadData(thread_id,
                                                    history_id,
                                                    messages_count,
                                                    snippet));
    return td;
};

void mail_cache::GThreadsStorage::update_db(
	EDataState ,
	CACHE_QUERY_RESULT_LIST<mail_cache::ThreadData>& r)
{
	QString sql_update, sql_insert;
    sql_update = QString("UPDATE %1gmail_thread SET history_id=?, messages_count=?, snippet=? WHERE thread_id=? AND acc_id=?")
        .arg(m_storage->metaPrefix());
    sql_insert = QString("INSERT INTO %1gmail_thread(history_id, messages_count, snippet, thread_id, acc_id) VALUES(?, ?, ?, ?, ?)")
        .arg(m_storage->metaPrefix());    

	std::function<bool(QSqlQuery*, mail_cache::ThreadData*)> execWithValues =
		[&](QSqlQuery* q,
			mail_cache::ThreadData* m) -> bool
	{
        q->addBindValue(m->historyId());
        q->addBindValue(m->messagesCount());
        q->addBindValue(m->snippet());
        q->addBindValue(m->id());
        q->addBindValue(m_storage->currentAccountId());

		return q->exec();
	};

	int updated_records = 0;
	int inserted_records = 0;

	for (auto& i : r)
	{
		thread_ptr& m = i;
		QSqlQuery* q = m_storage->prepareQuery(sql_update);
		if (!q)return;
		bool ok = execWithValues(q, m.get());
		if (!ok)
		{
			qWarning() << "ERROR. SQL update failed" << q->lastError().text() << i->id();
		}
		int rows_updated = q->numRowsAffected();
		if (rows_updated <= 0)
		{
			q = m_storage->prepareQuery(sql_insert);
			if (!q)return;
			ok = execWithValues(q, m.get());
			if (!ok)
			{
				qWarning() << "ERROR. SQL insert failed" << q->lastError().text() << i->id();
			}
			else
			{
				int rows_affected = q->numRowsAffected();
				if (rows_affected > 0)
					inserted_records++;
			}
		}
		else
		{
			updated_records++;
		}
	}
};

void mail_cache::GThreadsStorage::remove_db(const std::set<QString>& set_of_ids2remove)
{
	std::list<QString> ids2remove;
	for (auto& i : set_of_ids2remove)
	{
		ids2remove.push_back(i);
	}

	std::function<bool(const std::list<QString>& lst)> removeSublist = [&](const std::list<QString>& lst) -> bool
	{
		QString comma_ids = slist2commalist_decorated(lst);
		QString sql = QString("DELETE FROM %1gmail_thread WHERE thread_id IN(%2) AND acc_id=%3")
			.arg(m_storage->metaPrefix())
			.arg(comma_ids)
			.arg(m_storage->currentAccountId());
		QSqlQuery* q = m_storage->prepareQuery(sql);
		if (!q)return false;
		if (!q->exec()) return false;
		return true;
	};

	if (isValid())
	{
		if (!chunk_list_execution(ids2remove, removeSublist))
		{
			qWarning() << "ERROR. Failed to remove list of records.";
		}
	}    
};

bool mail_cache::GThreadsStorage::isValid()const 
{
	return m_storage->isValid();
};
