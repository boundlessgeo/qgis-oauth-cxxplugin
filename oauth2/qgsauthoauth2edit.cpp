/***************************************************************************
    begin                : July 13, 2016
    copyright            : (C) 2016 by Boundless Spatial, Inc. USA
    author               : Larry Shaffer
    email                : lshaffer at boundlessgeo dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsauthoauth2edit.h"
#include "ui_qgsauthoauth2edit.h"

#include <QDir>
#include <QFileDialog>

#include "qgsauthguiutils.h"
#include "qgsauthmanager.h"
#include "qgslogger.h"


QgsAuthOAuth2Edit::QgsAuthOAuth2Edit( QWidget *parent )
    : QgsAuthMethodEdit( parent )
    , mOAuthConfigCustom( 0 )
    , mDefinedConfigsCache( QgsStringMap() )
    , mParentName( 0 )
    , mValid( 0 )
    , mCurTab( 0 )
{
  setupUi( this );

  initGui();

  initConfigObjs();

  populateGrantFlows();
  updateGrantFlow( static_cast<int>( QgsAuthOAuth2Config::AuthCode ) ); // first index: Authorization Code

  populateAccessMethods();

  queryTableSelectionChanged();

  loadDefinedConfigs();

  setupConnections();

  loadFromOAuthConfig( mOAuthConfigCustom );
}

QgsAuthOAuth2Edit::~QgsAuthOAuth2Edit()
{
  deleteConfigObjs();
}

void QgsAuthOAuth2Edit::initGui()
{
  mParentName = parentNameField();

  frameNotify->setVisible( false );

  // TODO: add messagebar to notify frame?

  tabConfigs->setCurrentIndex( definedTab() );

  btnExport->setEnabled( false );

  chkbxTokenPersist->setChecked( false );

  grpbxAdvanced->setCollapsed( true );
  grpbxAdvanced->setFlat( false );
}

QLineEdit *QgsAuthOAuth2Edit::parentNameField()
{
  return parent() ? parent()->findChild<QLineEdit*>( "leName" ) : 0;
}

// slot
void QgsAuthOAuth2Edit::setupConnections()
{
  // Action and interaction connections
  connect( tabConfigs, SIGNAL( currentChanged( int ) ),
           this, SLOT( tabIndexChanged( int ) ) );

  connect( btnExport, SIGNAL( clicked() ),
           this, SLOT( exportOAuthConfig() ) );
  connect( btnImport, SIGNAL( clicked() ),
           this, SLOT( importOAuthConfig() ) );

  connect( tblwdgQueryPairs, SIGNAL( itemSelectionChanged() ),
           this, SLOT( queryTableSelectionChanged() ) );

  connect( btnAddQueryPair, SIGNAL( clicked() ),
           this, SLOT( addQueryPair() ) );
  connect( btnRemoveQueryPair, SIGNAL( clicked() ),
           this, SLOT( removeQueryPair() ) );

  connect( lstwdgDefinedConfigs, SIGNAL( currentItemChanged( QListWidgetItem*, QListWidgetItem* ) ),
           this, SLOT( currentDefinedItemChanged( QListWidgetItem*, QListWidgetItem* ) ) );

  connect( btnGetDefinedDirPath, SIGNAL( clicked() ),
           this, SLOT( getDefinedCustomDir() ) );
  connect( leDefinedDirPath, SIGNAL( textChanged( const QString& ) ),
           this, SLOT( definedCustomDirChanged( const QString& ) ) );

  // Custom config editing connections
  connect( cmbbxGrantFlow, SIGNAL( currentIndexChanged( int ) ),
           this, SLOT( updateGrantFlow( int ) ) ); // also updates GUI
  connect( pteDescription, SIGNAL( textChanged() ),
           this, SLOT( descriptionChanged() ) );
  connect( leRequestUrl, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setRequestUrl( const QString& ) ) );
  connect( leTokenUrl, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setTokenUrl( const QString& ) ) );
  connect( leRefreshTokenUrl, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setRefreshTokenUrl( const QString& ) ) );
  connect( leRedirectUrl, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setRedirectUrl( const QString& ) ) );
  connect( spnbxRedirectPort, SIGNAL( valueChanged( int ) ),
           mOAuthConfigCustom, SLOT( setRedirectPort( int ) ) );
  connect( leClientId, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setClientId( const QString& ) ) );
  connect( leClientSecret, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setClientSecret( const QString& ) ) );
  connect( leUsername, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setUsername( const QString& ) ) );
  connect( lePassword, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setPassword( const QString& ) ) );
  connect( leScope, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setScope( const QString& ) ) );
  connect( leState, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setState( const QString& ) ) );
  connect( leApiKey, SIGNAL( textChanged( const QString& ) ),
           mOAuthConfigCustom, SLOT( setApiKey( const QString& ) ) );
  connect( chkbxTokenPersist, SIGNAL( toggled( bool ) ),
           mOAuthConfigCustom, SLOT( setPersistToken( bool ) ) );
  connect( cmbbxAccessMethod, SIGNAL( currentIndexChanged( int ) ),
           this, SLOT( updateConfigAccessMethod( int ) ) );
  connect( spnbxRequestTimeout, SIGNAL( valueChanged( int ) ),
           mOAuthConfigCustom, SLOT( setRequestTimeout( int ) ) );

  connect( mOAuthConfigCustom, SIGNAL( validityChanged( bool ) ),
           this, SLOT( configValidityChanged() ) );

  if ( mParentName )
  {
    connect( mParentName, SIGNAL( textChanged( const QString& ) ),
             this, SLOT( configValidityChanged() ) );
  }
}

// slot
void QgsAuthOAuth2Edit::configValidityChanged()
{
  validateConfig();
  bool parentname = mParentName && !mParentName->text().isEmpty();
  btnExport->setEnabled( mValid && parentname );
}

bool QgsAuthOAuth2Edit::validateConfig()
{
  bool curvalid = ( onCustomTab() ? mOAuthConfigCustom->isValid() : !mDefinedId.isEmpty() );
  if ( mValid != curvalid )
  {
    mValid = curvalid;
    emit validityChanged( curvalid );
  }
  return curvalid;
}

QgsStringMap QgsAuthOAuth2Edit::configMap() const
{
  QgsStringMap configmap;
  bool ok = false;

  if ( onCustomTab() )
  {
    if ( !mOAuthConfigCustom || !mOAuthConfigCustom->isValid() )
    {
      QgsDebugMsg( QString( "FAILED to serialize OAuth config object: null or invalid object" ) );
      return configmap;
    }

    mOAuthConfigCustom->setQueryPairs( queryPairs() );

    QByteArray configtxt = mOAuthConfigCustom->saveConfigTxt( QgsAuthOAuth2Config::JSON, false, &ok );

    if ( !ok )
    {
      QgsDebugMsg( QString( "FAILED to serialize OAuth config object" ) );
      return configmap;
    }

    if ( configtxt.isEmpty() )
    {
      QgsDebugMsg( QString( "FAILED to serialize OAuth config object: content empty" ) );
      return configmap;
    }

    //###################### DO NOT LEAVE ME UNCOMMENTED #####################
    //QgsDebugMsg( QString( "SAVE oauth2config configtxt: \n\n%1\n\n" ).arg( QString( configtxt ) ) );
    //###################### DO NOT LEAVE ME UNCOMMENTED #####################

    configmap.insert( "oauth2config", QString( configtxt ) );
  }
  else if ( onDefinedTab() && !mDefinedId.isEmpty() )
  {
    configmap.insert( "definedid", mDefinedId );
    configmap.insert( "defineddirpath", leDefinedDirPath->text() );
    configmap.insert( "querypairs",
                      QgsAuthOAuth2Config::serializeFromVariant(
                        queryPairs(), QgsAuthOAuth2Config::JSON, false ) );
  }

  return configmap;
}

void QgsAuthOAuth2Edit::loadConfig( const QgsStringMap &configmap )
{
  clearConfig();

  mConfigMap = configmap;
  bool ok = false;

  //QgsDebugMsg( QString( "oauth2config: " ).arg( configmap.value( "oauth2config" ) ) );

  if ( configmap.contains( "oauth2config" ) )
  {
    tabConfigs->setCurrentIndex( customTab() );
    QByteArray configtxt = configmap.value( "oauth2config" ).toUtf8();
    if ( !configtxt.isEmpty() )
    {
      //###################### DO NOT LEAVE ME UNCOMMENTED #####################
      //QgsDebugMsg( QString( "LOAD oauth2config configtxt: \n\n%1\n\n" ).arg( QString( configtxt ) ) );
      //###################### DO NOT LEAVE ME UNCOMMENTED #####################

      if ( !mOAuthConfigCustom->loadConfigTxt( configtxt, QgsAuthOAuth2Config::JSON ) )
      {
        QgsDebugMsg( QString( "FAILED to load OAuth2 config into object" ) );
      }

      //###################### DO NOT LEAVE ME UNCOMMENTED #####################
      //QVariantMap vmap = mOAuthConfigCustom->mappedProperties();
      //QByteArray vmaptxt = QgsAuthOAuth2Config::serializeFromVariant(vmap, QgsAuthOAuth2Config::JSON, true );
      //QgsDebugMsg( QString( "LOAD oauth2config vmaptxt: \n\n%1\n\n" ).arg( QString( vmaptxt ) ) );
      //###################### DO NOT LEAVE ME UNCOMMENTED #####################

      // could only be loading defaults at this point
      loadFromOAuthConfig( mOAuthConfigCustom );
    }
    else
    {
      QgsDebugMsg( QString( "FAILED to load OAuth2 config: empty config txt" ) );
    }
  }
  else if ( configmap.contains( "definedid" ) )
  {
    tabConfigs->setCurrentIndex( definedTab() );
    QString definedid = configmap.value( "definedid" );
    setCurrentDefinedConfig( definedid );
    if ( !definedid.isEmpty() )
    {
      if ( !configmap.value( "defineddirpath" ).isEmpty() )
      {
        // this will trigger a reload of dirs and a reselection of any existing defined id
        leDefinedDirPath->setText( configmap.value( "defineddirpath" ) );
      }
      else
      {
        QgsDebugMsg( QString( "No custom defined dir path to load OAuth2 config" ) );
        selectCurrentDefinedConfig();
      }

      QByteArray querypairstxt = configmap.value( "querypairs" ).toUtf8();
      if ( !querypairstxt.isNull() && !querypairstxt.isEmpty() )
      {
        QVariantMap querypairsmap =
          QgsAuthOAuth2Config::variantFromSerialized( querypairstxt, QgsAuthOAuth2Config::JSON, &ok );
        if ( ok )
        {
          populateQueryPairs( querypairsmap );
        }
        else
        {
          QgsDebugMsg( QString( "No query pairs to load OAuth2 config: failed to parse" ) );
        }
      }
      else
      {
        QgsDebugMsg( QString( "No query pairs to load OAuth2 config: empty text" ) );
      }
    }
    else
    {
      QgsDebugMsg( QString( "FAILED to load a defined ID for OAuth2 config" ) );
    }
  }

  validateConfig();
}

void QgsAuthOAuth2Edit::resetConfig()
{
  loadConfig( mConfigMap );
}

void QgsAuthOAuth2Edit::clearConfig()
{
  // restore defaults to config objs
  mOAuthConfigCustom->setToDefaults();

  clearQueryPairs();

  // clear any set predefined location
  leDefinedDirPath->clear();

  // reload predefined table
  loadDefinedConfigs();

  loadFromOAuthConfig( mOAuthConfigCustom );
}

// slot
void QgsAuthOAuth2Edit::loadFromOAuthConfig( const QgsAuthOAuth2Config *config )
{
  if ( !config )
  {
    return;
  }

  // load relative to config type
  if ( config->configType() == QgsAuthOAuth2Config::Custom )
  {
    if ( config->isValid() )
    {
      tabConfigs->setCurrentIndex( customTab() );
    }
    pteDescription->setPlainText( config->description() );
    leRequestUrl->setText( config->requestUrl() );
    leTokenUrl->setText( config->tokenUrl() );
    leRefreshTokenUrl->setText( config->refreshTokenUrl() );
    leRedirectUrl->setText( config->redirectUrl() );
    spnbxRedirectPort->setValue( config->redirectPort() );
    leClientId->setText( config->clientId() );
    leClientSecret->setText( config->clientSecret() );
    leUsername->setText( config->username() );
    lePassword->setText( config->password() );
    leScope->setText( config->scope() );
    leState->setText( config->state() );
    leApiKey->setText( config->apiKey() );

    // advanced
    chkbxTokenPersist->setChecked( config->persistToken() );
    cmbbxAccessMethod->setCurrentIndex( static_cast<int>( config->accessMethod() ) );
    spnbxRequestTimeout->setValue( config->requestTimeout() );

    populateQueryPairs( config->queryPairs() );

    updateGrantFlow( static_cast<int>( config->grantFlow() ) );
  }

  validateConfig();
}

// slot
void QgsAuthOAuth2Edit::tabIndexChanged( int indx )
{
  mCurTab = indx;
  validateConfig();
}

// slot
void QgsAuthOAuth2Edit::populateGrantFlows()
{
  cmbbxGrantFlow->addItem( QgsAuthOAuth2Config::grantFlowString( QgsAuthOAuth2Config::AuthCode ),
                           static_cast<int>( QgsAuthOAuth2Config::AuthCode ) );
  cmbbxGrantFlow->addItem( QgsAuthOAuth2Config::grantFlowString( QgsAuthOAuth2Config::Implicit ),
                           static_cast<int>( QgsAuthOAuth2Config::Implicit ) );
  cmbbxGrantFlow->addItem( QgsAuthOAuth2Config::grantFlowString( QgsAuthOAuth2Config::ResourceOwner ),
                           static_cast<int>( QgsAuthOAuth2Config::ResourceOwner ) );
}

// slot
void QgsAuthOAuth2Edit::definedCustomDirChanged( const QString &path )
{
  QFileInfo pinfo( path );
  bool ok = pinfo.exists() and pinfo.isDir();

  leDefinedDirPath->setStyleSheet( ok ? "" : QgsAuthGuiUtils::redTextStyleSheet() );

  if ( ok )
  {
    loadDefinedConfigs();
  }
}

// slot
void QgsAuthOAuth2Edit::setCurrentDefinedConfig( const QString &id )
{
  mDefinedId = id;
  QgsDebugMsg( QString( "Set defined ID: %1" ).arg( id ) );
  validateConfig();
}

void QgsAuthOAuth2Edit::currentDefinedItemChanged( QListWidgetItem * cur, QListWidgetItem * prev )
{
  Q_UNUSED( prev )

  QgsDebugMsg( QString( "Entered" ) );

  QString id = cur->data( Qt::UserRole ).toString();
  if ( !id.isEmpty() )
  {
    setCurrentDefinedConfig( id );
  }
}

// slot
void QgsAuthOAuth2Edit::selectCurrentDefinedConfig()
{
  if ( mDefinedId.isEmpty() )
  {
    return;
  }

  if ( !onDefinedTab() )
  {
    tabConfigs->setCurrentIndex( definedTab() );
  }

  lstwdgDefinedConfigs->selectionModel()->clearSelection();

  for ( int i = 0; i < lstwdgDefinedConfigs->count(); ++i )
  {
    QListWidgetItem *itm = lstwdgDefinedConfigs->item( i );

    if ( itm->data( Qt::UserRole ).toString() == mDefinedId )
    {
      lstwdgDefinedConfigs->setCurrentItem( itm, QItemSelectionModel::Select );
      break;
    }
  }
}

// slot
void QgsAuthOAuth2Edit::getDefinedCustomDir()
{
  QString extradir = QFileDialog::getExistingDirectory( this, tr( "Select extra directory to parse" ),
                     QDir::homePath(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks );
  this->raise();
  this->activateWindow();

  if ( extradir.isEmpty() )
  {
    return;
  }
  leDefinedDirPath->setText( extradir );
}

void QgsAuthOAuth2Edit::initConfigObjs()
{
  mOAuthConfigCustom = new QgsAuthOAuth2Config( this );
  mOAuthConfigCustom->setConfigType( QgsAuthOAuth2Config::Custom );
  mOAuthConfigCustom->setToDefaults();
}

void QgsAuthOAuth2Edit::deleteConfigObjs()
{
  delete mOAuthConfigCustom;
  mOAuthConfigCustom = nullptr;
}

// slot
void QgsAuthOAuth2Edit::updateDefinedConfigsCache()
{
  QString extradir = leDefinedDirPath->text();
  mDefinedConfigsCache.clear();
  mDefinedConfigsCache = QgsAuthOAuth2Config::mappedOAuth2ConfigsCache( extradir );
}

// slot
void QgsAuthOAuth2Edit::loadDefinedConfigs()
{
  lstwdgDefinedConfigs->blockSignals( true );
  lstwdgDefinedConfigs->clear();
  lstwdgDefinedConfigs->blockSignals( false );

  updateDefinedConfigsCache();

  QgsStringMap::const_iterator i = mDefinedConfigsCache.constBegin();
  while ( i != mDefinedConfigsCache.constEnd() )
  {
    QgsAuthOAuth2Config *config = new QgsAuthOAuth2Config( this );
    if ( !config->loadConfigTxt( i.value().toUtf8(), QgsAuthOAuth2Config::JSON ) )
    {
      QgsDebugMsg( QString( "FAILED to load config for ID: %1" ).arg( i.key() ) );
      config->deleteLater();
      continue;
    }

    QString grantflow = QgsAuthOAuth2Config::grantFlowString( config->grantFlow() );

    QString name = QString( "%1 (%2): %3" )
                   .arg( config->name(), grantflow, config->description() );

    QString tip = tr( "ID: %1\nGrant flow: %2\nDescription: %3" )
                  .arg( i.key(), grantflow, config->description() );

    QListWidgetItem* itm = new QListWidgetItem( lstwdgDefinedConfigs );
    itm->setText( name );
    itm->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    itm->setData( Qt::UserRole, QVariant( i.key() ) );
    itm->setData( Qt::ToolTipRole, QVariant( tip ) );
    lstwdgDefinedConfigs->addItem( itm );

    config->deleteLater();
    ++i;
  }

  if ( lstwdgDefinedConfigs->count() == 0 )
  {
    QListWidgetItem* itm = new QListWidgetItem( lstwdgDefinedConfigs );
    itm->setText( tr( "No predefined configurations found on disk" ) );
    QFont f( itm->font() );
    f.setItalic( true );
    itm->setFont( f );
    itm->setFlags( Qt::NoItemFlags );
    lstwdgDefinedConfigs->addItem( itm );
  }

  selectCurrentDefinedConfig();
}

bool QgsAuthOAuth2Edit::onCustomTab() const
{
  return mCurTab == customTab();
}

bool QgsAuthOAuth2Edit::onDefinedTab() const
{
  return mCurTab == definedTab();
}

// slot
void QgsAuthOAuth2Edit::updateGrantFlow( int indx )
{
  if ( cmbbxGrantFlow->currentIndex() != indx )
  {
    cmbbxGrantFlow->blockSignals( true );
    cmbbxGrantFlow->setCurrentIndex( indx );
    cmbbxGrantFlow->blockSignals( false );
  }

  QgsAuthOAuth2Config::GrantFlow flow =
    static_cast<QgsAuthOAuth2Config::GrantFlow>( cmbbxGrantFlow->itemData( indx ).toInt() );
  mOAuthConfigCustom->setGrantFlow( flow );

  // bool authcode = ( flow == QgsAuthOAuth2Config::AuthCode );
  bool implicit = ( flow == QgsAuthOAuth2Config::Implicit );
  bool resowner = ( flow == QgsAuthOAuth2Config::ResourceOwner );

  lblRequestUrl->setVisible( !resowner );
  leRequestUrl->setVisible( !resowner );
  if ( resowner ) leRequestUrl->setText( QString() );

  lblRedirectUrl->setVisible( !resowner );
  frameRedirectUrl->setVisible( !resowner );

  lblClientSecret->setVisible( !implicit );
  leClientSecret->setVisible( !implicit );
  if ( implicit ) leClientSecret->setText( QString() );

  leClientId->setPlaceholderText( resowner ? tr( "Optional" ) : tr( "Required" ) );
  leClientSecret->setPlaceholderText( resowner ? tr( "Optional" ) : tr( "Required" ) );


  lblUsername->setVisible( resowner );
  leUsername->setVisible( resowner );
  if ( !resowner ) leUsername->setText( QString() );
  lblPassword->setVisible( resowner );
  lePassword->setVisible( resowner );
  if ( !resowner ) lePassword->setText( QString() );
}

// slot
void QgsAuthOAuth2Edit::exportOAuthConfig()
{
  if ( !onCustomTab() || !mValid )
  {
    return;
  }

  QSettings settings;
  QString recentdir = settings.value( "UI/lastAuthSaveFileDir", QDir::homePath() ).toString();
  QString configpath = QFileDialog::getSaveFileName(
                         this, tr( "Save OAuth2 Config File" ), recentdir, "OAuth2 config files (*.json)" );
  this->raise();
  this->activateWindow();

  if ( configpath.isEmpty() )
  {
    return;
  }
  settings.setValue( "UI/lastAuthSaveFileDir", QFileInfo( configpath ).absoluteDir().path() );

  // give it a kind of random id for re-importing
  mOAuthConfigCustom->setId( QgsAuthManager::instance()->uniqueConfigId() );

  mOAuthConfigCustom->setQueryPairs( queryPairs() );

  if ( mParentName && !mParentName->text().isEmpty() )
  {
    mOAuthConfigCustom->setName( mParentName->text() );
  }

  if ( !QgsAuthOAuth2Config::writeOAuth2Config( configpath, mOAuthConfigCustom,
       QgsAuthOAuth2Config::JSON, true ) )
  {
    QgsDebugMsg( "FAILED to export OAuth2 config file" );
  }
  // clear temp changes
  mOAuthConfigCustom->setId( QString::null );
  mOAuthConfigCustom->setName( QString::null );
}

// slot
void QgsAuthOAuth2Edit::importOAuthConfig()
{
  if ( !onCustomTab() )
  {
    return;
  }

  QString configfile =
    QgsAuthGuiUtils::getOpenFileName( this, tr( "Select OAuth2 Config File" ), "OAuth2 config files (*.json)" );
  this->raise();
  this->activateWindow();

  QFileInfo importinfo( configfile );
  if ( configfile.isEmpty() || !importinfo.exists() )
  {
    return;
  }

  QByteArray configtxt;
  QFile cfile( configfile );
  bool ret = cfile.open( QIODevice::ReadOnly | QIODevice::Text );
  if ( ret )
  {
    configtxt = cfile.readAll();
  }
  else
  {
    QgsDebugMsg( QString( "FAILED to open config for reading: %1" ).arg( configfile ) );
    cfile.close();
    return;
  }
  cfile.close();

  if ( configtxt.isEmpty() )
  {
    QgsDebugMsg( QString( "EMPTY read of config: %1" ).arg( configfile ) );
    return;
  }

  QgsStringMap configmap;
  configmap.insert( "oauth2config", QString( configtxt ) );
  loadConfig( configmap );
}

// slot
void QgsAuthOAuth2Edit::descriptionChanged()
{
  mOAuthConfigCustom->setDescription( pteDescription->toPlainText() );
}

// slot
void QgsAuthOAuth2Edit::populateAccessMethods()
{
  cmbbxAccessMethod->addItem( QgsAuthOAuth2Config::accessMethodString( QgsAuthOAuth2Config::Header ),
                              static_cast<int>( QgsAuthOAuth2Config::Header ) );
  cmbbxAccessMethod->addItem( QgsAuthOAuth2Config::accessMethodString( QgsAuthOAuth2Config::Form ),
                              static_cast<int>( QgsAuthOAuth2Config::Form ) );
  cmbbxAccessMethod->addItem( QgsAuthOAuth2Config::accessMethodString( QgsAuthOAuth2Config::Query ),
                              static_cast<int>( QgsAuthOAuth2Config::Query ) );
}

// slot
void QgsAuthOAuth2Edit::updateConfigAccessMethod( int indx )
{
  mOAuthConfigCustom->setAccessMethod( static_cast<QgsAuthOAuth2Config::AccessMethod>( indx ) );
}

void QgsAuthOAuth2Edit::addQueryPairRow( const QString &key, const QString &val )
{
  int rowCnt = tblwdgQueryPairs->rowCount();
  tblwdgQueryPairs->insertRow( rowCnt );

  Qt::ItemFlags itmFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable
                           | Qt::ItemIsEditable | Qt::ItemIsDropEnabled;

  QTableWidgetItem* keyItm = new QTableWidgetItem( key );
  keyItm->setFlags( itmFlags );
  tblwdgQueryPairs->setItem( rowCnt, 0, keyItm );

  QTableWidgetItem* valItm = new QTableWidgetItem( val );
  keyItm->setFlags( itmFlags );
  tblwdgQueryPairs->setItem( rowCnt, 1, valItm );
}

// slot
void QgsAuthOAuth2Edit::populateQueryPairs( const QVariantMap &querypairs, bool append )
{
  if ( !append )
  {
    clearQueryPairs();
  }

  QVariantMap::const_iterator i = querypairs.constBegin();
  while ( i != querypairs.constEnd() )
  {
    addQueryPairRow( i.key(), i.value().toString() );
    ++i;
  }
}

// slot
void QgsAuthOAuth2Edit::queryTableSelectionChanged()
{
  bool hassel = tblwdgQueryPairs->selectedItems().count() > 0;
  btnRemoveQueryPair->setEnabled( hassel );
}

void QgsAuthOAuth2Edit::updateConfigQueryPairs()
{
  mOAuthConfigCustom->setQueryPairs( queryPairs() );
}

QVariantMap QgsAuthOAuth2Edit::queryPairs() const
{
  QVariantMap querypairs;
  for ( int i = 0; i < tblwdgQueryPairs->rowCount(); ++i )
  {
    if ( tblwdgQueryPairs->item( i, 0 )->text().isEmpty() )
    {
      continue;
    }
    querypairs.insert( tblwdgQueryPairs->item( i, 0 )->text(),
                       QVariant( tblwdgQueryPairs->item( i, 1 )->text() ) );
  }
  return querypairs;
}

// slot
void QgsAuthOAuth2Edit::addQueryPair()
{
  addQueryPairRow( QString(), QString() );
  tblwdgQueryPairs->setFocus();
  tblwdgQueryPairs->setCurrentCell( tblwdgQueryPairs->rowCount() - 1, 0 );
  tblwdgQueryPairs->edit( tblwdgQueryPairs->currentIndex() );
}

// slot
void QgsAuthOAuth2Edit::removeQueryPair()
{
  tblwdgQueryPairs->removeRow( tblwdgQueryPairs->currentRow() );
}

// slot
void QgsAuthOAuth2Edit::clearQueryPairs()
{
  for ( int i = tblwdgQueryPairs->rowCount(); i > 0 ; --i )
  {
    tblwdgQueryPairs->removeRow( i - 1 );
  }
}
